#
# Copyright (C) 2025 ETH Zurich and University of Bologna
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# Author: Chi Zhang <chizhang@ethz.ch>

import onnx
from onnx import helper, TensorProto

def analyze_to_onnx(kernel_flow, spaceA_hbm_plan, spaceB_hbm_plan):
    nodes_dict = {}
    tensor_map = {}
    for kernel_name in kernel_flow:
        #1. Get list of kernel inputs and outputs
        kernel_input_list = []
        kernel_output_list = []
        for k, v in kernel_flow[kernel_name].items():
            if k != "type" and k != "cfg" and k != "info":
                if k == 'o' or 'output' in k:
                    kernel_output_list.append(v['name'])
                else:
                    kernel_input_list.append(v['name'])
                    pass
                pass
            pass

        #2. process tensor name mapping
        mapped_kernel_input_list = []
        mapped_kernel_output_list = []
        for inp in kernel_input_list:
            if inp in tensor_map:
                # set input use
                tensor_map[inp]['in'][-1] = 1
            else:
                # create new tag for tensor mapping
                tensor_map[inp] = {}
                tensor_map[inp]['map'] = [inp]
                tensor_map[inp]['in'] = [1]
                tensor_map[inp]['out'] = [0]
                pass
            # add latest name in mapped input list
            mapped_kernel_input_list.append(tensor_map[inp]['map'][-1])
            pass
        for oup in kernel_output_list:
            if oup in tensor_map:
                # we need to add a new name to it
                new_name = oup + f"_{len(tensor_map[oup]['map'])}"
                tensor_map[oup]['map'].append(new_name)
                tensor_map[oup]['in'].append(0)
                tensor_map[oup]['out'].append(1)
            else:
                # create new tag for tensor mapping
                tensor_map[oup] = {}
                tensor_map[oup]['map'] = [oup]
                tensor_map[oup]['in'] = [0]
                tensor_map[oup]['out'] = [1]
                pass
            # add latest name in mapped output list
            mapped_kernel_output_list.append(tensor_map[oup]['map'][-1])
            pass

        #3. add to nodes_dict
        nodes_dict[kernel_name] = {
            'name'      : kernel_name,
            'type'      : kernel_flow[kernel_name]['type'],
            'inputs'    : mapped_kernel_input_list,
            'outputs'   : mapped_kernel_output_list
        }
        pass
    return tensor_map, nodes_dict
    pass

def create_onnx_tensors(tensor_map, spaceA_hbm_plan, spaceB_hbm_plan, onnx_dtype=TensorProto.FLOAT16):
    #1. map rename to actual name
    remap = {}
    for k, v in tensor_map.items():
        for x in range(len(v['map'])):
            rename = v['map'][x]
            remap[rename] = {}
            remap[rename]['real name'] = k
            remap[rename]['catagory'] = 'intermediate' if (v['in'][x] == 1 and v['out'][x] == 1) else 'input' if (v['in'][x] == 1 and v['out'][x] == 0) else 'output'
            pass
        pass

    #2. Create tensor dict
    tensor_dict                 = {}
    tensor_dict['constant']     = []
    tensor_dict['input']        = []
    tensor_dict['output']       = []
    tensor_dict['intermediate'] = []
    for tensor_name, remap_info in remap.items():
        real_name = remap_info['real name']

        # Get HBM plan information
        if real_name in spaceA_hbm_plan:
            plan_info = spaceA_hbm_plan[real_name]
            direction = 'spaceA'
        else:
            plan_info = spaceB_hbm_plan[real_name]
            direction = 'spaceB'
            pass

        # Create onnx tensors
        shape = list(plan_info['shape'])
        if 'view' in plan_info:
            shape = list(plan_info['view'])
            pass
        doc_string = f"[info] On: {direction} | Start: {plan_info['addr']: #x} | Size: {plan_info['size']: #x} bytes"
        if 'bias' in tensor_name or 'weight' in tensor_name or 'table' in tensor_name:
            tensor_dict['constant'].append(helper.make_tensor(tensor_name, onnx_dtype, shape,[]))
        elif remap_info['catagory'] == 'input':
            tensor_dict['input'].append(helper.make_tensor_value_info(tensor_name, onnx_dtype, shape, doc_string=doc_string))
        elif remap_info['catagory'] == 'output':
            tensor_dict['output'].append(helper.make_tensor_value_info(tensor_name, onnx_dtype, shape, doc_string=doc_string))
        else:
            tensor_dict['intermediate'].append(helper.make_tensor_value_info(tensor_name, onnx_dtype, shape, doc_string=doc_string))
            pass
        pass

    return tensor_dict
    pass

def create_onnx_node_list(nodes_dict):
    node_list = []
    for k, v in nodes_dict.items():
        node_list.append(helper.make_node(op_type=v['name'], inputs=v['inputs'], outputs=v['outputs'], name=v['type']))
        pass
    return node_list
    pass

def create_onnx_graph(kernel_flow, spaceA_hbm_plan, spaceB_hbm_plan, save_path):
    #1. analyze flow
    tensor_map, nodes_dict = analyze_to_onnx(kernel_flow, spaceA_hbm_plan, spaceB_hbm_plan)
    tensor_dict = create_onnx_tensors(tensor_map, spaceA_hbm_plan, spaceB_hbm_plan)
    node_list = create_onnx_node_list(nodes_dict)

    #2. build graph, model and onnx plot
    graph = helper.make_graph(
        nodes=node_list,
        name='LLM Flow',
        inputs=tensor_dict['input'],
        outputs=tensor_dict['output'],
        initializer=tensor_dict['constant'],
        value_info=tensor_dict['intermediate']
    )
    model = helper.make_model(
        graph,
        producer_name='CustomONNXGenerator',
        opset_imports=[helper.make_operatorsetid("", 18)]  # standard opset 18
    )
    onnx.save(model, save_path)
    pass