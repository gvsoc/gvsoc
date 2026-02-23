/*
 * Copyright (C) 2024 ETH Zurich and University of Bologna
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
 * Authors: Chi Zhang, ETH Zurich (chizhang@iis.ee.ethz.ch)
 */

#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include <queue>
#include "../c2c_platform.hpp"

class Mesh2D : public vp::Component
{

public:
    Mesh2D(vp::ComponentConf &config);
    enum Algo { XY = 0 };
    enum Dirc { LOCAL = 0, WEST = 1, NORTH = 2, EAST = 3, SOUTH = 4 };

private:
    void reset(bool active);
    static vp::IoReqStatus      req(vp::Block *__this, vp::IoReq *req);
    int                         xy_routing(int dest_x, int dest_y);
    vp::Trace                   trace;
    vp::IoSlave                 input_itf;
    int                         x_dim;
    int                         y_dim;
    int                         x_pos;
    int                         y_pos;
    int                         ralgo;
};

Mesh2D::Mesh2D(vp::ComponentConf &config)
    : vp::Component(config)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);
    this->input_itf.set_req_meth(&Mesh2D::req);
    this->new_slave_port("in", &this->input_itf);

    this->x_dim = this->get_js_config()->get("x_dim")->get_int();
    this->y_dim = this->get_js_config()->get("y_dim")->get_int();
    this->x_pos = this->get_js_config()->get("x_pos")->get_int();
    this->y_pos = this->get_js_config()->get("y_pos")->get_int();
    this->ralgo = this->get_js_config()->get("ralgo")->get_int();
}

void Mesh2D::reset(bool active)
{
    if (active)
    {
        this->trace.msg(vp::Trace::LEVEL_TRACE, "Reset done\n");
    }
}

int Mesh2D::xy_routing(int dest_x, int dest_y)
{
    if (dest_x == this->x_pos && dest_y == this->y_pos)
    {
        //To Local
        return LOCAL;
    } else if (dest_x == this->x_pos)
    {
        //Y Direction
        return (dest_y > this->y_pos)? NORTH : SOUTH;
    } else {
        //X Direction
        return (dest_x > this->x_pos)? EAST : WEST;
    }
}

vp::IoReqStatus Mesh2D::req(vp::Block *__this, vp::IoReq *req)
{
    Mesh2D *_this = (Mesh2D *)__this;

    int REQ_SOUR_ID     = *(int *)req->arg_get(C2CPlatform::REQ_SOUR_ID);
    int REQ_DEST_ID     = *(int *)req->arg_get(C2CPlatform::REQ_DEST_ID);
    int REQ_IS_ACK      = *(int *)req->arg_get(C2CPlatform::REQ_IS_ACK);
    int REQ_IS_LAST     = *(int *)req->arg_get(C2CPlatform::REQ_IS_LAST);
    int REQ_SIZE        = *(int *)req->arg_get(C2CPlatform::REQ_SIZE);
    int REQ_WRITE       = *(int *)req->arg_get(C2CPlatform::REQ_WRITE);
    int REQ_PORT_ID     = *(int *)req->arg_get(C2CPlatform::REQ_PORT_ID);

    int dest_x          = REQ_DEST_ID % _this->x_dim;
    int dest_y          = REQ_DEST_ID / _this->x_dim;
    int port            = 0;

    if (_this->ralgo == XY)
    {
        port = _this->xy_routing(dest_x, dest_y);
    } else {
        _this->trace.fatal("[Topology] Routing Algorithm %d not supported \n", _this->ralgo);
    }

    *req->arg_get(C2CPlatform::REQ_PORT_ID) = (void *)port;

    return vp::IO_REQ_OK;
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Mesh2D(config);
}

