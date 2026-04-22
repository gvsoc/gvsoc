/*
 * Copyright (C) 2020 GreenWaves Technologies, SAS, ETH Zurich and
 *                    University of Bologna
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Authors: Germain Haugou, GreenWaves Technologies (germain.haugou@greenwaves-technologies.com)
 */

#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include <vp/itf/wire.hpp>
#include <stdio.h>
#include <string.h>
#include <stdint.h>


class MemoryMemcheck
{
public:
    MemoryMemcheck(void *data);
    uint64_t alloc(uint64_t offset, uint64_t size);
    uint64_t free(uint64_t offset, uint64_t size);

private:
    void *data;
};


class MemoryMemcheckBuffer
{
public:
    bool enable;
    uintptr_t base;
    size_t size;
};


class Memory : public vp::Component
{

public:
    Memory(vp::ComponentConf &config);

    void reset(bool active);

    static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req);

    uint64_t memcheck_alloc(uint64_t ptr, uint64_t size);
    uint64_t memcheck_free(uint64_t ptr, uint64_t size);

private:
    static void power_ctrl_sync(vp::Block *__this, bool value);
    static void meminfo_sync_back(vp::Block *__this, void **value);
    static void meminfo_sync(vp::Block *__this, void *value);
    static void memcheck_sync(vp::Block *__this, MemoryMemcheckBuffer *info);
    vp::IoReqStatus handle_write(uint64_t addr, uint64_t size, uint8_t *data, uint8_t *memcheck_data);
    vp::IoReqStatus handle_read(uint64_t addr, uint64_t size, uint8_t *data, uint8_t *memcheck_data);
    vp::IoReqStatus handle_atomic(uint64_t addr, uint64_t size, uint8_t *in_data, uint8_t *out_data,
        vp::IoReqOpcode opcode, int initiator, uint8_t *in_memcheck_data, uint8_t *out_memcheck_data);
    // Check if an access to the specified offset falls into a valid buffer
    bool memcheck_is_valid_address(uint64_t offset);
    // In case of a faulting access, find the closest valid buffer to the offset
    void memcheck_find_closest_buffer(uint64_t offset, uint64_t &distance, uint64_t &buffer_offset, uint64_t &buffer_size);
    void memcheck_buffer_setup(uint64_t base, uint64_t size, bool enable);
    bool check_buffer_access(uint64_t offset, uint64_t size, bool is_write);

    vp::Trace trace;
    vp::IoSlave in;

    uint64_t size = 0;
    bool check = false;
    int width_bits = 0;
    int latency;

    uint8_t *mem_data;
    uint8_t *memcheck_data = NULL;
    uint8_t *check_mem;
    uint8_t *memcheck_valid_flags = NULL;

    int64_t next_packet_start;

    vp::WireSlave<bool> power_ctrl_itf;
    vp::WireSlave<void *> meminfo_itf;
    vp::WireSlave<MemoryMemcheckBuffer *> memcheck_itf;

    bool power_trigger;
    bool powered_up;

    vp::PowerSource access_byte_power;
    vp::PowerSource background_power;

    vp::ClockEvent *power_event;
    int64_t last_access_timestamp;

    // Load-reserved reservation table
    std::map<uint64_t, uint64_t> res_table;

    uint64_t memcheck_base;
    uint64_t memcheck_virtual_base;
    uint64_t memcheck_expansion_factor;
};



Memory::Memory(vp::ComponentConf &config)
    : vp::Component(config)
{
    traces.new_trace("trace", &trace, vp::DEBUG);
    in.set_req_meth(&Memory::req);
    new_slave_port("input", &in);

    this->power_ctrl_itf.set_sync_meth(&Memory::power_ctrl_sync);
    new_slave_port("power_ctrl", &this->power_ctrl_itf);

    this->meminfo_itf.set_sync_back_meth(&Memory::meminfo_sync_back);
    this->meminfo_itf.set_sync_meth(&Memory::meminfo_sync);
    new_slave_port("meminfo", &this->meminfo_itf);

    this->memcheck_itf.set_sync_meth(&Memory::memcheck_sync);
    new_slave_port("memcheck", &this->memcheck_itf);

    js::Config *js_config = get_js_config()->get("power_trigger");
    this->power_trigger = js_config != NULL && js_config->get_bool();

    power.new_power_source("leakage", &background_power, this->get_js_config()->get("**/background"));
    power.new_power_source("access_byte", &access_byte_power, this->get_js_config()->get("**/access_byte"));

    this->size = this->get_js_config()->get("size")->get_int();
    this->check = get_js_config()->get_child_bool("check");
    this->width_bits = get_js_config()->get_child_int("width_bits");
    this->latency = get_js_config()->get_child_int("latency");
    int align = get_js_config()->get_child_int("align");

    trace.msg("Building Memory (size: 0x%x, check: %d)\n", size, check);

    if (align)
    {
        mem_data = (uint8_t *)aligned_alloc(align, size);
    }
    else
    {
        mem_data = (uint8_t *)calloc(size, 1);
        if (mem_data == NULL) throw std::bad_alloc();
    }

    // Special option to check for uninitialized accesses
    if (check)
    {
        check_mem = new uint8_t[(size + 7) / 8];
    }
    else
    {
        check_mem = NULL;
    }

    if (this->traces.get_trace_engine()->is_memcheck_enabled())
    {
        this->memcheck_data = (uint8_t *)calloc(size, 1);
        if (this->memcheck_data == NULL) throw std::bad_alloc();

        int memcheck_id = this->get_js_config()->get_child_int("memcheck_id");
        if (memcheck_id != -1)
        {
            this->new_service("memcheck_memory" + std::to_string(memcheck_id),
                new MemoryMemcheck((void *)this));

            this->memcheck_expansion_factor = this->get_js_config()->get_child_int("memcheck_expansion_factor");
            int memcheck_size = size * this->memcheck_expansion_factor;
            this->memcheck_valid_flags = (uint8_t *)calloc((memcheck_size + 7) / 8, 1);
            if (this->memcheck_valid_flags == NULL) throw std::bad_alloc();

            this->memcheck_base = this->get_js_config()->get_child_int("memcheck_base");
            this->memcheck_virtual_base = this->get_js_config()->get_child_int("memcheck_virtual_base");
        }
    }

    // Initialize the Memory with a special value to detect uninitialized
    // variables.
    // Only do it for small memories to not slow down too much simulation init.
    if (size < (2<<24))
    {
        memset(mem_data, 0x57, size);
    }

    // Preload the Memory
    js::Config *stim_file_conf = this->get_js_config()->get("stim_file");
    if (stim_file_conf != NULL)
    {
        string path = stim_file_conf->get_str();
        if (path != "")
        {
            trace.msg("Preloading Memory with stimuli file (path: %s)\n", path.c_str());

            FILE *file = fopen(path.c_str(), "rb");
            if (file == NULL)
            {
                this->trace.fatal("Unable to open stim file: %s, %s\n", path.c_str(), strerror(errno));
                return;
            }
            if (fread(this->mem_data, 1, size, file) == 0)
            {
                this->trace.fatal("Failed to read stim file: %s, %s\n", path.c_str(), strerror(errno));
                return;
            }
        }
    }

    this->background_power.leakage_power_start();
    // this->background_power.dynamic_power_start();
    this->last_access_timestamp = -1;
}



vp::IoReqStatus Memory::req(vp::Block *__this, vp::IoReq *req)
{
    Memory *_this = (Memory *)__this;

    uint64_t offset = req->get_addr();
    uint8_t *data = req->get_data();
    uint64_t size = req->get_size();

    _this->trace.msg("Memory access (offset: 0x%x, size: 0x%x, is_write: %d, op: %d)\n", offset, size, req->get_is_write(), req->get_opcode());

    req->inc_latency(_this->latency);

    // Impact the Memory bandwith on the packet
    if (_this->width_bits != 0)
    {
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
        int duration = MAX(size >> _this->width_bits, 1);
        req->set_duration(duration);
        int64_t cycles = _this->clock.get_cycles();
        int64_t diff = _this->next_packet_start - cycles;
        if (diff > 0)
        {
            _this->trace.msg("Delayed packet (latency: %ld)\n", diff);
            req->inc_latency(diff);
        }
        _this->next_packet_start = MAX(_this->next_packet_start, cycles) + duration;
    }

    //Add power consumption for accesses
    for(int i=0; i<size; i++)
    {
        _this->access_byte_power.account_energy_quantum();
    }

#ifdef VP_TRACE_ACTIVE
    if (_this->power_trigger)
    {
        if (req->get_is_write() && size == 4 && offset == 0)
        {
            if (*(uint32_t *)data == 0xabbaabba)
            {
                _this->power.get_engine()->start_capture();
            }
            else if (*(uint32_t *)data == 0xdeadcaca)
            {
                static int measure_index = 0;
                _this->power.get_engine()->stop_capture();
                double dynamic_power, static_power;
                fprintf(stderr, "@power.measure_%d@%f@\n", measure_index++, _this->power.get_engine()->get_average_power(dynamic_power, static_power));
            }
        }
    }
#endif

    if (offset + size > _this->size)
    {
        _this->trace.force_warning("Received out-of-bound request (reqAddr: 0x%x, reqSize: 0x%x, memSize: 0x%x)\n", offset, size, _this->size);
        return vp::IO_REQ_INVALID;
    }

    if (req->get_opcode() == vp::IoReqOpcode::READ)
    {
#ifdef VP_MEMCHECK_ACTIVE
        return _this->handle_read(offset, size, data, req->get_memcheck_data());
#else
        return _this->handle_read(offset, size, data, NULL);
#endif
    }
    else if (req->get_opcode() == vp::IoReqOpcode::WRITE)
    {
#ifdef VP_MEMCHECK_ACTIVE
        return _this->handle_write(offset, size, data, req->get_memcheck_data());
#else
        return _this->handle_write(offset, size, data, NULL);
#endif
    }
    else
    {
#ifdef CONFIG_ATOMICS
#ifdef VP_MEMCHECK_ACTIVE
        return _this->handle_atomic(offset, size, data, req->get_second_data(), req->get_opcode(),
            req->get_initiator(), req->get_memcheck_data(), req->get_second_memcheck_data());
#else
        return _this->handle_atomic(offset, size, data, req->get_second_data(), req->get_opcode(),
            req->get_initiator(), NULL, NULL);
#endif
#else
        _this->trace.force_warning("Received unsupported atomic operation\n");
        return vp::IO_REQ_INVALID;
#endif
    }
}



vp::IoReqStatus Memory::handle_write(uint64_t offset, uint64_t size, uint8_t *data, uint8_t *req_memcheck_data)
{
    // Writes on powered-down memory are silently ignored
    if (!this->powered_up)
    {
        return vp::IO_REQ_OK;
    }

#ifdef VP_MEMCHECK_ACTIVE
    if (this->check_buffer_access(offset, size, true))
    {
        return vp::IO_REQ_INVALID;
    }

    if (this->memcheck_data != NULL)
    {
        if (req_memcheck_data != NULL)
        {
            memcpy((void *)&this->memcheck_data[offset], (void *)req_memcheck_data, size);
        }
        else
        {
            // If there is no memcheck data, it means the model which is writting does not have
            // support to it. Set all data to valid to avoid false negative
            memset((void *)&this->memcheck_data[offset], -1, size);
        }
    }
#endif

    if (data)
    {
        memcpy((void *)&this->mem_data[offset], (void *)data, size);
    }

    return vp::IO_REQ_OK;
}


bool Memory::memcheck_is_valid_address(uint64_t offset)
{
    // Valid bytes are monitored through an array, one bit per byte
    size_t valid_byte = offset >> 3;
    int valid_bit = offset & 0x7;
    return (this->memcheck_valid_flags[valid_byte] >> valid_bit) & 1;
}

void Memory::memcheck_find_closest_buffer(uint64_t offset, uint64_t &distance,
    uint64_t &buffer_offset, uint64_t &buffer_size)
{
    // First go through the memory in descending order from the current offset to see if we find a
    // valid buffer before the offset
    uint64_t valid_before_offset;
    uint64_t valid_before_offset_last;
    uint64_t distance_before = 0;
    if (offset > 0)
    {
        uint64_t current_offset = offset;

        // First look for the first valid bit to get end of buffer
        do
        {
            current_offset--;

            if (this->memcheck_is_valid_address(current_offset))
            {
                valid_before_offset_last = current_offset;
                valid_before_offset = current_offset;
                distance_before = offset - current_offset;
                break;
            }
        } while (current_offset != 0);

        // And then for the first invalid bit to get start of buffer
        if (current_offset > 0)
        {
            do
            {
                current_offset--;
                if (this->memcheck_is_valid_address(current_offset))
                {
                    valid_before_offset = current_offset;
                }
                else
                {
                    break;
                }
            } while (current_offset != 0);
        }
    }

    // First go through the memory in ascending order from the current offset to see if we find a
    // valid buffer after the offset
    uint64_t valid_after_offset;
    uint64_t valid_after_offset_last;
    uint64_t distance_after = 0;
    // First look for the first valid bit to get start of buffer
    for (uint64_t current_offset = offset; current_offset<this->size; current_offset++)
    {
        if (this->memcheck_is_valid_address(current_offset))
        {
            valid_after_offset = current_offset;
            valid_after_offset_last = current_offset;
            distance_after = current_offset - offset;
            break;
        }
    }

    // And then for the first invalid bit to get end of buffer
    for (uint64_t current_offset = valid_after_offset; current_offset<this->size; current_offset++)
    {
        if (this->memcheck_is_valid_address(current_offset))
        {
            valid_after_offset_last = current_offset;
        }
        else
        {
            break;
        }
    }

    if (distance_before == 0 && distance_after == 0)
    {
        distance = 0;
        return;
    }

    // Finally return the one which is closer
    if (distance_before == 0 || (distance_after != 0 && distance_after < distance_before))
    {
        distance = distance_after;
        buffer_offset = valid_after_offset;
        buffer_size = valid_after_offset_last - valid_after_offset + 1;
    }
    else
    {
        distance = distance_before;
        buffer_offset = valid_before_offset;
        buffer_size = valid_before_offset_last - valid_before_offset + 1;
    }
}

bool Memory::check_buffer_access(uint64_t offset, uint64_t size, bool is_write)
{
    if (this->memcheck_valid_flags != NULL)
    {
        // Go through all bytes of the access to see if one is not valid
        for (int i=0; i<size; i++)
        {
            uint64_t current_offset = offset + i;
            bool is_valid = this->memcheck_is_valid_address(current_offset);

            if (!is_valid)
            {
                // If not, get the closest valid buffer and throw a warning to help the user
                // understand better the overflow
                uint64_t buffer_offset, buffer_size, distance;
                this->memcheck_find_closest_buffer(current_offset, distance, buffer_offset, buffer_size);

                this->trace.force_warning_no_error("%s access outside buffer "
                    "(offset: 0x%x)\n", is_write ? "Write" : "Read", current_offset);

                if (distance == 0)
                {
                    this->trace.force_warning_no_error("%s access with no buffer\n");
                }
                else
                {
                    bool is_before = buffer_offset > current_offset;

                    this->trace.force_warning_no_error("%s access is %ld byte(s) %s buffer (buffer_offset: 0x%llx, buffer_size: 0x%llx)\n",
                        is_write ? "Write" : "Read", distance, is_before ? "before" : "after", buffer_offset, buffer_size);

                    return true;
                }
            }
        }
    }

    return false;
}

vp::IoReqStatus Memory::handle_read(uint64_t offset, uint64_t size, uint8_t *data, uint8_t *req_memcheck_data)
{
    // Reads on powered-down memory return 0
    if (!this->powered_up)
    {
        memset((void *)data, 0, size);
        return vp::IO_REQ_OK;
    }

#ifdef VP_MEMCHECK_ACTIVE

    if (this->check_buffer_access(offset, size, false))
    {
        return vp::IO_REQ_INVALID;
    }

    if (this->memcheck_data != NULL)
    {
        if (req_memcheck_data != NULL)
        {
            memcpy((void *)req_memcheck_data, (void *)&this->memcheck_data[offset], size);
        }
    }
#endif

    if (data)
    {
        memcpy((void *)data, (void *)&this->mem_data[offset], size);
    }

    return vp::IO_REQ_OK;
}


static inline int64_t get_signed_value(int64_t val, int bits)
{
    return ((int64_t)val) << (64 - bits) >> (64 - bits);
}


vp::IoReqStatus Memory::handle_atomic(uint64_t addr, uint64_t size, uint8_t *in_data,
    uint8_t *out_data, vp::IoReqOpcode opcode, int initiator, uint8_t *in_memcheck_data,
    uint8_t *out_memcheck_data)
{
    int64_t operand = 0;
    int64_t prev_val = 0;
    int64_t result;
    bool is_write = true;

    memcpy((uint8_t *)&operand, in_data, size);

    this->handle_read(addr, size, (uint8_t *)&prev_val, out_memcheck_data);

    if (size < 8)
    {
        operand = get_signed_value(operand, size*8);
        prev_val = get_signed_value(prev_val, size*8);
    }

    switch (opcode)
    {
        case vp::IoReqOpcode::LR:
            this->res_table[initiator] = addr;
            is_write = false;
            break;
        case vp::IoReqOpcode::SC:
        {
            auto it = this->res_table.find(initiator);
            if (it != this->res_table.end() && it->second == addr)
            {
                // Valid reservation --> clear all others as we are going to write
                for (auto &it : this->res_table) {
                    if (it.second >= addr && it.second < addr+size)
                    {
                        it.second = -1;
                    }
                }
                result   = operand;
                prev_val = 0;
            }
            else
            {
                is_write = false;
                prev_val = 1;
            }
            break;
        }
        case vp::IoReqOpcode::SWAP:
            result = operand;
            break;
        case vp::IoReqOpcode::ADD:
            result = prev_val + operand;
            break;
        case vp::IoReqOpcode::XOR:
            result = prev_val ^ operand;
            break;
        case vp::IoReqOpcode::AND:
            result = prev_val & operand;
            break;
        case vp::IoReqOpcode::OR:
            result = prev_val | operand;
            break;
        case vp::IoReqOpcode::MIN:
            result = prev_val < operand ? prev_val : operand;
            break;
        case vp::IoReqOpcode::MAX:
            result = prev_val > operand ? prev_val : operand;
            break;
        case vp::IoReqOpcode::MINU:
            result = (uint64_t) prev_val < (uint64_t) operand ? prev_val : operand;
            break;
        case vp::IoReqOpcode::MAXU:
            result = (uint64_t) prev_val > (uint64_t) operand ? prev_val : operand;
            break;
        default:
            return vp::IO_REQ_INVALID;
    }

    memcpy(out_data, (uint8_t *)&prev_val, size);
    if (is_write)
    {
        this->handle_write(addr, size, (uint8_t *)&result, in_memcheck_data);
    }

    return vp::IO_REQ_OK;
}



void Memory::reset(bool active)
{
    if (active)
    {
        this->next_packet_start = 0;
        this->powered_up = true;
    }
}



void Memory::power_ctrl_sync(vp::Block *__this, bool value)
{
    Memory *_this = (Memory *)__this;
    _this->powered_up = value;
}



void Memory::meminfo_sync_back(vp::Block *__this, void **value)
{
    Memory *_this = (Memory *)__this;
    *value = _this->mem_data;
}



void Memory::meminfo_sync(vp::Block *__this, void *value)
{
    Memory *_this = (Memory *)__this;
    _this->mem_data = (uint8_t *)value;
}



void Memory::memcheck_sync(vp::Block *__this, MemoryMemcheckBuffer *info)
{

}


void Memory::memcheck_buffer_setup(uint64_t base, uint64_t size, bool enable)
{
#ifdef VP_MEMCHECK_ACTIVE
    if (this->memcheck_valid_flags != NULL)
    {
        this->trace.msg(vp::Trace::LEVEL_INFO, "%s valid buffer (offset: 0x%lx, size: 0x%lx)\n",
            enable ? "Adding" : "Removing", base, size);

        if (base + size >= this->size * this->memcheck_expansion_factor)
        {
            this->trace.force_warning("Trying to %s invalid buffer  (offset: 0x%lx, size: 0x%lx, mem_size: 0x%lx)\n",
                enable ? "add" : "remove", base, size, this->size);
            return;
        }

        for (int i=0; i<size; i++)
        {
            uint64_t offset = base + i;
            int valid_byte = offset >> 3;
            int valid_bit = offset & 0x7;
            if (enable)
            {
                this->memcheck_valid_flags[valid_byte] |= 1 << valid_bit;
            }
            else
            {
                this->memcheck_valid_flags[valid_byte] &= ~(1 << valid_bit);
            }
        }
    }
#endif
}

uint64_t Memory::memcheck_alloc(uint64_t ptr, uint64_t size)
{
#ifdef VP_MEMCHECK_ACTIVE
    if (this->memcheck_valid_flags != NULL)
    {
        uint64_t virtual_offset = (ptr - this->memcheck_base) * this->memcheck_expansion_factor +
            size * (this->memcheck_expansion_factor  / 2) ;
        uint64_t virtual_ptr = virtual_offset + this->memcheck_virtual_base;

        this->memcheck_buffer_setup(virtual_offset, size, true);

        return virtual_ptr;
    }
#endif

    return ptr;
}

uint64_t Memory::memcheck_free(uint64_t virtual_ptr, uint64_t size)
{
#ifdef VP_MEMCHECK_ACTIVE
    if (this->memcheck_valid_flags != NULL)
    {
        uint64_t virtual_offset = virtual_ptr - this->memcheck_virtual_base;
        uint64_t offset = (virtual_offset - size * (this->memcheck_expansion_factor  / 2)) / this->memcheck_expansion_factor + this->memcheck_base;

        this->memcheck_buffer_setup(virtual_offset, size, false);

        return offset;
    }
#endif

    return virtual_ptr;
}

uint64_t MemoryMemcheck::alloc(uint64_t ptr, uint64_t size)
{
    Memory *memory = (Memory *)this->data;
    return memory->memcheck_alloc(ptr, size);
}

uint64_t MemoryMemcheck::free(uint64_t ptr, uint64_t size)
{
    Memory *memory = (Memory *)this->data;
    return memory->memcheck_free(ptr, size);
}

MemoryMemcheck::MemoryMemcheck(void *data)
{
    this->data = data;
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Memory(config);
}
