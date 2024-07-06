#include <iostream>
#include <vector>

std::vector<std::vector<int>> initialize_timing_array(int rows, int cols, int buffer_h, int buffer_w, int tcdms_bw) {
    std::vector<std::vector<int>> timing_array(rows, std::vector<int>(cols, 0));
    int base_value = (buffer_w * buffer_h) / tcdms_bw;
    int remainder = (buffer_w * buffer_h) % tcdms_bw;
    int increment = base_value + (remainder > 0 ? 1 : 0);
    for (int col = 0; col < rows; col++) {
        for (int row = 0; row < cols; row++) {
            timing_array[col][row] = increment;
        }
    }
    return timing_array;
}

std::vector<std::vector<int>> cube_unit_timing_array_calculation_folded_tile(
    int x_row_lefts, int x_row_tiles, int x_col_lefts, int x_col_tiles,
    int w_row_lefts, int w_row_tiles, int w_col_lefts, int w_col_tiles,
    int z_row_lefts, int z_row_tiles, int z_col_lefts, int z_col_tiles,
    int buffer_h, int buffer_w, int tcdms_bw) {

    std::vector<std::vector<int>> y_access_timing_array = initialize_timing_array(z_col_tiles, z_row_tiles, buffer_h, buffer_w, tcdms_bw);
    std::vector<std::vector<int>> x_access_timing_array = initialize_timing_array(x_col_tiles, x_row_tiles, buffer_h, buffer_w, tcdms_bw);
    std::vector<std::vector<int>> w_access_timing_array = initialize_timing_array(w_col_tiles, w_row_tiles, buffer_w, buffer_w, tcdms_bw);
    std::vector<std::vector<int>> z_access_timing_array = initialize_timing_array(z_col_tiles, z_row_tiles, buffer_h, buffer_w, tcdms_bw);

    if (z_row_lefts > 0) {
        for (int col = 0; col < z_col_tiles; col++) {
            int val = (z_row_lefts * buffer_h) / tcdms_bw + ((z_row_lefts * buffer_h) % tcdms_bw > 0 ? 1 : 0);
            y_access_timing_array[col][z_row_tiles - 1] = val;
            z_access_timing_array[col][z_row_tiles - 1] = val;
        }
    }

    if (x_row_lefts > 0) {
        for (int col = 0; col < x_col_tiles; col++) {
            int val = (x_row_lefts * buffer_h) / tcdms_bw + ((x_row_lefts * buffer_h) % tcdms_bw > 0 ? 1 : 0);
            x_access_timing_array[col][x_row_tiles - 1] = val;
        }
    }

    if (w_row_lefts > 0) {
        for (int col = 0; col < w_col_tiles; col++) {
            int val = (w_row_lefts * buffer_w) / tcdms_bw + ((w_row_lefts * buffer_w) % tcdms_bw > 0 ? 1 : 0);
            w_access_timing_array[col][w_row_tiles - 1] = val;
        }
    }

    if (z_col_lefts > 0) {
        for (int row = 0; row < z_row_tiles; row++) {
            int val = (buffer_w * z_col_lefts) / tcdms_bw + ((buffer_w * z_col_lefts) % tcdms_bw > 0 ? 1 : 0);
            y_access_timing_array[z_col_tiles - 1][row] = val;
            z_access_timing_array[z_col_tiles - 1][row] = val;
        }
    }

    if (x_col_lefts > 0) {
        for (int row = 0; row < x_row_tiles; row++) {
            int val = (buffer_w * x_col_lefts) / tcdms_bw + ((buffer_w * x_col_lefts) % tcdms_bw > 0 ? 1 : 0);
            x_access_timing_array[x_col_tiles - 1][row] = val;
        }
    }

    if (w_col_lefts > 0) {
        for (int row = 0; row < w_row_tiles; row++) {
            int val = (buffer_w * w_col_lefts) / tcdms_bw + ((buffer_w * w_col_lefts) % tcdms_bw > 0 ? 1 : 0);
            w_access_timing_array[w_col_tiles - 1][row] = val;
        }
    }

    if (z_row_lefts > 0 && z_col_lefts > 0) {
        int val = (z_col_lefts * z_row_lefts) / tcdms_bw + ((z_col_lefts * z_row_lefts) % tcdms_bw > 0 ? 1 : 0);
        y_access_timing_array[z_col_tiles - 1][z_row_tiles - 1] = val;
        z_access_timing_array[z_col_tiles - 1][z_row_tiles - 1] = val;
    }

    if (x_row_lefts > 0 && x_col_lefts > 0) {
        int val = (x_col_lefts * x_row_lefts) / tcdms_bw + ((x_col_lefts * x_row_lefts) % tcdms_bw > 0 ? 1 : 0);
        x_access_timing_array[x_col_tiles - 1][x_row_tiles - 1] = val;
    }

    if (w_row_lefts > 0 && w_col_lefts > 0) {
        int val = (w_col_lefts * w_row_lefts) / tcdms_bw + ((w_col_lefts * w_row_lefts) % tcdms_bw > 0 ? 1 : 0);
        w_access_timing_array[w_col_tiles - 1][w_row_tiles - 1] = val;
    }

    return y_access_timing_array;
}

int cube_unit_runtime(int cube_height, int cube_width, int cube_pipeline, int cube_elem_size_byte, int tcdm_bank_width_byte, int nb_tcdm_banks, int m_size, int n_size, int k_size) {
    int buffer_h = cube_height;
    int buffer_w = cube_width * (cube_pipeline + 1);
    int tcdms_bw = tcdm_bank_width_byte * nb_tcdm_banks / cube_elem_size_byte;

    int x_row_lefts = n_size % buffer_w;
    int x_row_tiles = n_size / buffer_w + (x_row_lefts > 0 ? 1 : 0);

    int x_col_lefts = m_size % buffer_h;
    int x_col_tiles = m_size / buffer_h + (x_col_lefts > 0 ? 1 : 0);

    int w_row_lefts = k_size % buffer_w;
    int w_row_tiles = k_size / buffer_w + (w_row_lefts > 0 ? 1 : 0);

    int w_col_lefts = x_row_lefts;
    int w_col_tiles = x_row_tiles;

    int z_row_lefts = w_row_lefts;
    int z_row_tiles = w_row_tiles;

    int z_col_lefts = x_col_lefts;
    int z_col_tiles = x_col_tiles;

    auto y_access_timing_array = cube_unit_timing_array_calculation_folded_tile(
        x_row_lefts, x_row_tiles, x_col_lefts, x_col_tiles,
        w_row_lefts, w_row_tiles, w_col_lefts, w_col_tiles,
        z_row_lefts, z_row_tiles, z_col_lefts, z_col_tiles,
        buffer_h, buffer_w, tcdms_bw
    );

    int runtime = 0;

    int preload_time = y_access_timing_array[0][0];
    runtime += preload_time;

    int z_store_time = y_access_timing_array[0][0];
    for (int row = 0; row < z_row_tiles; row++) {
        for (int col = 0; col < z_col_tiles; col++) {
            for (int i = 0; i < x_row_tiles; i++) {
                int cube_unit_mac_time = buffer_w * (cube_pipeline + 1);

                int cube_unit_access_time = 0;

                int _row = row;
                int _col = col;
                int _i = i + 1;
                if (_i >= x_row_tiles) {
                    _i = 0;
                    _col = col + 1;
                    if (_col >= z_col_tiles) {
                        _col = 0;
                        _row = row + 1;
                    }
                }

                if (_row < z_row_tiles) {
                    cube_unit_access_time = y_access_timing_array[_col][_i];
                    if (_i == 0) {
                        cube_unit_access_time += y_access_timing_array[_col][_row];
                    }
                }

                if (i == 0 && (row != 0 || col != 0)) {
                    cube_unit_access_time += z_store_time;
                    z_store_time = y_access_timing_array[col][row];
                }

                if (cube_unit_mac_time >= cube_unit_access_time) {
                    runtime += cube_unit_mac_time;
                } else {
                    runtime += cube_unit_access_time;
                }
            }
        }
    }

    int final_store_time = 0;
    int cube_unit_store_time_limit = buffer_w;
    int cube_unit_store_time = y_access_timing_array[z_col_tiles - 1][z_row_tiles - 1];
    if (cube_unit_store_time > cube_unit_store_time_limit) {
        final_store_time += cube_unit_store_time;
    } else {
        final_store_time += cube_unit_store_time_limit;
    }
    runtime += final_store_time;

    return runtime;
}

int main() {
    int cube_height = 4;
    int cube_width = 4;
    int cube_pipeline = 1;
    int cube_elem_size_byte = 4;
    int tcdm_bank_width_byte = 8;
    int nb_tcdm_banks = 16;
    int m_size = 256;
    int n_size = 256;
    int k_size = 256;

    int runtime = cube_unit_runtime(cube_height, cube_width, cube_pipeline, cube_elem_size_byte, tcdm_bank_width_byte, nb_tcdm_banks, m_size, n_size, k_size);
    std::cout << "Runtime: " << runtime << std::endl;

    return 0;
}
