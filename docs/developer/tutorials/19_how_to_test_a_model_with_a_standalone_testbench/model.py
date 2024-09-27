import gvsoc.systree

class Model(gvsoc.systree.Component):
    def __init__(self, parent: gvsoc.systree.Component, name: str):

        super().__init__(parent, name)

        self.add_sources(['model.cpp'])

    def i_INPUT(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'input', signature='wire<int>')

    def o_OUTPUT(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('output', itf, signature='wire<int>')