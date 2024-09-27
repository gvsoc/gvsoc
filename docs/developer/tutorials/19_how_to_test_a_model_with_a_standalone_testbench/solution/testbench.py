import gvsoc.systree

class Testbench(gvsoc.systree.Component):
    def __init__(self, parent: gvsoc.systree.Component, name: str):

        super().__init__(parent, name)

        self.add_sources(['testbench.cpp'])

    def i_DUT_OUTPUT(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'dut_output', signature='wire<int>')

    def o_DUT_INPUT(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('dut_input', itf, signature='wire<int>')
