Interfaces
==========

Memory-mapped IO request
........................

.. image:: images/io_request.png

.. code-block:: cpp

    vp::IoReqStatus MyComp::handle_request(vp::Block *__this, vp::IoReq *req)
    {
        MyComp *_this = (vp::Block *)__this;
        if (req->get_addr() == 0 && req->get_size() == 4)
        {
            if (req->get_is_write())
            {
                _this->reg_value = *(uint32_t *)req->get_data();
            }
            else
            {
                *(uint32_t *)req->get_data() = _this->reg_value;
            }
            return vp::IO_REQ_OK;
        }
        return vp::IO_REQ_INVALID;
    }

.. code-block:: cpp

    vp::IoReqStatus MyComp::handle_request(vp::Block *__this, vp::IoReq *req)
    {
        MyComp *_this = (vp::Block *)__this;
        if (_this->fifo.size_left() < req->get_size())
        {
            _this->denied_queue.push(req);
            return vp::IO_REQ_DENIED;
        }
        else
        {
            _this->fifo.push(req);
            return vp::IO_REQ_PENDING;
        }
    }

.. code-block:: cpp

    vp::IoReq *MyComp::pop_request()
    {
        vp::IoReq *req = this->fifo.pop();

        if (this->fifo.size_left() >= this->denied_queue.head()->get_size())
        {
            vp::IoReq *denied_req = _this->denied_queue.pop();
            this->fifo.push(denied_req);
            req->get_resp_port()->grant(denied_req):
        }

        return req;
    }

.. code-block:: cpp

    void MyComp::handle_requests(void *_this, vp::ClockEvent *event)
    {
        MyComp *_this = (vp::Block *)__this;
        vp::IoReq *req = _this->pop_request();
        if (req)
        {
            vp::IoReqStatus status = _this->output.req(req);
            if (status == vp::IO_REQ_OK || status == vp::IO_REQ_INVALID)
            {
                req->status = status;
                req->get_resp_port()->resp(req);
            }
        }
    }