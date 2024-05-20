import numpy as np
import os
def multiply_and_sum_vectors(input: np.ndarray, weight: np.ndarray) -> np.int32:
    if len(input) != len(weight):
        raise ValueError(f"length mismatch between input and weight\n")
    input = np.array(input, dtype=np.uint8)
    weight = np.array(weight, dtype=np.int8)

    # Element wise multiplication -> int32
    product = np.multiply(input, weight, dtype=np.int32)

    # accumulation in int32
    output = np.array([np.sum(product, dtype=np.int32)], dtype=np.int32)
    
    return output

def get_data_type(data: np.ndarray) -> str:
    if(data.dtype == np.uint8):
        return "uint8_t"
    elif(data.dtype == np.int8):
        return "int8_t" 
    elif(data.dtype == np.int32):
        return "int32_t"
    else:
        raise ValueError(f"The datatype is not handled\n")

def generate_header_file(vector: np.ndarray, filename: str, vector_name: str) -> None:
    with open(filename, 'w') as f:
        f.write("#ifndef __{0}_H__\n".format(vector_name.upper()))
        f.write("#define __{0}_H__\n".format(vector_name.upper()))
        f.write("#define STIM_{0}_SIZE {1}\n".format(vector_name.upper(), len(vector)))
        f.write("PI_L1 {0} {1}[]={{\n".format(get_data_type(vector), vector_name))
        f.write(", ".join(map(str, vector)))
        f.write("};\n\n")
        f.write("#endif //__{0}_H__\n".format(vector_name.upper()))
    f.close()


if __name__ == "__main__":
    dimension = 8
    input = np.random.randint(0, 256, size=dimension, dtype=np.uint8)
    weight= np.random.randint(-128, 128, size=dimension, dtype=np.int8)
    expected_output = multiply_and_sum_vectors(input, weight)
    actual_output = np.zeros(len(expected_output), dtype=np.int32)
    weight_folded = (weight + 128).astype(np.uint8)

    generate_header_file(input, os.path.join(os.getcwd(), "inc", "input.h"), "hwpe_input")
    generate_header_file(weight_folded, os.path.join(os.getcwd(), "inc", "weight.h"), "hwpe_weight")
    generate_header_file(expected_output, os.path.join(os.getcwd(), "inc", "expected_output.h"), "hwpe_expected_output")
    generate_header_file(actual_output, os.path.join(os.getcwd(), "inc", "actual_output.h"), "hwpe_actual_output")


    print(f"input: {input}")
    print(f"weight: {weight}")
    print(f"expected_output: {expected_output}")
