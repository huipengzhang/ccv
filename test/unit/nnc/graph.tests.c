#include "ccv.h"
#include "case.h"
#include "ccv_case.h"
#include "ccv_nnc_case.h"
#include "nnc/ccv_nnc.h"
#include "3rdparty/dsfmt/dSFMT.h"

static void _ccv_nnc_custom_24_loss_exec(const ccv_nnc_cmd_t cmd, const ccv_nnc_hint_t hint, const int flags, ccv_nnc_tensor_t* const* inputs, const int input_size, ccv_nnc_tensor_t** outputs, const int output_size)
{
	int i;
	assert(input_size == 1);
	const ccv_nnc_tensor_t* m = inputs[0];
	assert(output_size == 1);
	ccv_nnc_tensor_t* g = outputs[0];
	for (i = 0; i < 21 * 31 * 4; i++)
		g->data.f32[i] = m->data.f32[i] - (i == 24);
}

TEST_CASE("run simple graph network")
{
	ccv_nnc_init();
	ccv_nnc_graph_t* graph = ccv_nnc_graph_new();
	ccv_nnc_tensor_param_t a_params = {
		.type = CCV_TENSOR_CPU_MEMORY,
		.format = CCV_TENSOR_FORMAT_NHWC,
		.dim = {
			2, 21, 31,
		},
	};
	ccv_nnc_tensor_param_t b_params = {
		.type = CCV_TENSOR_CPU_MEMORY,
		.format = CCV_TENSOR_FORMAT_NHWC,
		.dim = {
			4, 21, 31,
		},
	};
	ccv_nnc_tensor_param_t h_params = a_params;
	ccv_nnc_tensor_param_t g_params = b_params;
	ccv_nnc_tensor_param_t w_params = {
		.type = CCV_TENSOR_CPU_MEMORY,
		.format = CCV_TENSOR_FORMAT_NHWC,
		.dim = {
			2, 3, 5, 4,
		},
	};
	ccv_nnc_tensor_param_t bias_params = {
		.type = CCV_TENSOR_CPU_MEMORY,
		.format = CCV_TENSOR_FORMAT_NHWC,
		.dim = {
			4,
		},
	};
	ccv_nnc_cmd_param_t cmd_params = {
		.size = {
			.dim = {
				2, 3, 5,
			},
		},
		.convolutional = {
			.count = 4,
		},
	};
	ccv_nnc_hint_t hint = ccv_nnc_hint_guess(cmd_params, &a_params, 1, &b_params, 1);
	ccv_nnc_tensor_t* a = ccv_nnc_tensor_new(0, a_params, 0);
	ccv_nnc_tensor_t* b = ccv_nnc_tensor_new(0, b_params, 0);
	ccv_nnc_cmd_t forw_cmd = ccv_nnc_cmd(CCV_NNC_COMPUTE_CONVOLUTIONAL_FORWARD, 0, cmd_params, 0);
	ccv_nnc_tensor_t* w = ccv_nnc_tensor_new(0, w_params, 0);
	ccv_nnc_tensor_t* bias = ccv_nnc_tensor_new(0, bias_params, 0);
	dsfmt_t dsfmt;
	dsfmt_init_gen_rand(&dsfmt, 1);
	int i;
	for (i = 0; i < 2 * 3 * 5 * 4; i++)
		w->data.f32[i] = (dsfmt_genrand_open_close(&dsfmt) * 2 - 1) * 1.41421356237 / sqrtf(21 * 31 * 2 + 21 * 31 * 4);
	float denom = (21 * 31 * 2 - 1) * 21 * 31 * 2;
	for (i = 0; i < 21 * 31 * 2; i++)
		a->data.f32[i] = (float)(i - 21 * 31) / denom;
	for (i = 0; i < 4; i++)
		bias->data.f32[i] = 0;
	ccv_nnc_tensor_t* forw_inlets[] = {
		a,
		w,
		bias,
	};
	ccv_nnc_tensor_t* forw_outlets[] = {
		b,
	};
	ccv_nnc_graph_exec_t forw_node = ccv_nnc_graph_deferred_exec(graph, forw_cmd, hint, 0, forw_inlets, 3, forw_outlets, 1);
	ccv_nnc_cmd_t softmax_cmd = ccv_nnc_cmd(CCV_NNC_COMPUTE_SOFTMAX_FORWARD, 0, cmd_params, 0);
	ccv_nnc_tensor_t* m = ccv_nnc_tensor_new(0, b_params, 0);
	ccv_nnc_tensor_t* max_inlets[] = {
		b,
	};
	ccv_nnc_tensor_t* max_outlets[] = {
		m,
	};
	ccv_nnc_graph_exec_t softmax_node = ccv_nnc_graph_deferred_exec(graph, softmax_cmd, hint, 0, max_inlets, 1, max_outlets, 1);
	ccv_nnc_tensor_t* g = ccv_nnc_tensor_new(0, g_params, 0);
	ccv_nnc_cmd_t loss_cmd = ccv_nnc_cmd(CCV_NNC_COMPUTE_CUSTOM, _ccv_nnc_custom_24_loss_exec, cmd_params, 0);
	ccv_nnc_tensor_t* loss_inlets[] = {
		m,
	};
	ccv_nnc_tensor_t* loss_outlets[] = {
		g,
	};
	ccv_nnc_graph_exec_t loss_node = ccv_nnc_graph_deferred_exec(graph, loss_cmd, hint, 0, loss_inlets, 1, loss_outlets, 1);
	ccv_nnc_cmd_t back_cmd = ccv_nnc_cmd(CCV_NNC_COMPUTE_CONVOLUTIONAL_BACKWARD, 0, cmd_params, 0);
	ccv_nnc_tensor_t* gw = ccv_nnc_tensor_new(0, w_params, 0);
	ccv_nnc_tensor_t* gbias = ccv_nnc_tensor_new(0, bias_params, 0);
	ccv_nnc_tensor_t* h = ccv_nnc_tensor_new(0, h_params, 0);
	ccv_nnc_tensor_t* back_inlets[] = {
		g,
		a,
		w,
	};
	ccv_nnc_tensor_t* back_outlets[] = {
		gw,
		gbias,
		h,
	};
	ccv_nnc_graph_exec_t back_node = ccv_nnc_graph_deferred_exec(graph, back_cmd, hint, 0, back_inlets, 3, back_outlets, 3);
	// All nodes are created, now to concat the graph.
	ccv_nnc_graph_exec_concat(graph, forw_node, softmax_node);
	ccv_nnc_graph_exec_concat(graph, softmax_node, loss_node);
	ccv_nnc_graph_exec_concat(graph, loss_node, back_node);
	ccv_nnc_graph_exec_t source_nodes[] = {
		forw_node,
	};
	ccv_nnc_graph_exec_t destination_nodes[] = {
		back_node,
	};
	ccv_nnc_graph_run(graph, 0, source_nodes, 1, destination_nodes, 1);
	ccv_nnc_graph_free(graph);
	/* At this point, do the computation with a different set of tensors and then compare */
	ccv_nnc_tensor_t* vb = ccv_nnc_tensor_new(0, b_params, 0);
	ccv_nnc_tensor_t* vforw_outlets[] = {
		vb,
	};
	ccv_nnc_cmd_exec(forw_cmd, hint, 0, forw_inlets, 3, vforw_outlets, 1);
	REQUIRE_TENSOR_EQ(b, vb, "Graph computed forward pass result should be the same.");
	ccv_nnc_tensor_t* vm = ccv_nnc_tensor_new(0, b_params, 0);
	ccv_nnc_tensor_t* vmax_inlets[] = {
		vb,
	};
	ccv_nnc_tensor_t* vmax_outlets[] = {
		vm,
	};
	ccv_nnc_cmd_exec(softmax_cmd, hint, 0, vmax_inlets, 1, vmax_outlets, 1);
	REQUIRE_TENSOR_EQ(m, vm, "Graph computed softmax pass result should be the same.");
	ccv_nnc_tensor_t* vg = ccv_nnc_tensor_new(0, g_params, 0);
	for (i = 0; i < 21 * 31 * 4; i++)
		vg->data.f32[i] = vm->data.f32[i] - (i == 24);
	REQUIRE_TENSOR_EQ(g, vg, "Graph computed custom loss result should be the same.");
	ccv_nnc_tensor_t* vgw = ccv_nnc_tensor_new(0, w_params, 0);
	ccv_nnc_tensor_t* vgbias = ccv_nnc_tensor_new(0, bias_params, 0);
	ccv_nnc_tensor_t* vh = ccv_nnc_tensor_new(0, h_params, 0);
	ccv_nnc_tensor_t* vback_inlets[] = {
		vg,
		a,
		w,
	};
	ccv_nnc_tensor_t* vback_outlets[] = {
		vgw,
		vgbias,
		vh,
	};
	ccv_nnc_cmd_exec(back_cmd, hint, 0, vback_inlets, 3, vback_outlets, 3);
	REQUIRE_TENSOR_EQ(gbias, vgbias, "Graph computed backward pass weight delta should be the same.");
	REQUIRE_TENSOR_EQ(gw, vgw, "Graph computed backward pass bias delta should be the same.");
	REQUIRE_TENSOR_EQ(h, vh, "Graph computed backward pass result should be the same.");
	// free all the tensor data.
	ccv_nnc_tensor_free(a);
	ccv_nnc_tensor_free(b);
	ccv_nnc_tensor_free(m);
	ccv_nnc_tensor_free(g);
	ccv_nnc_tensor_free(h);
	ccv_nnc_tensor_free(w);
	ccv_nnc_tensor_free(bias);
	ccv_nnc_tensor_free(gw);
	ccv_nnc_tensor_free(gbias);
	ccv_nnc_tensor_free(vb);
	ccv_nnc_tensor_free(vm);
	ccv_nnc_tensor_free(vg);
	ccv_nnc_tensor_free(vh);
	ccv_nnc_tensor_free(vgw);
	ccv_nnc_tensor_free(vgbias);
}

#include "case_main.h"
