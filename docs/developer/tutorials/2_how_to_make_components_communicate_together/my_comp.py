
import gvsoc.systree

class MyComp(gvsoc.systree.Component):
    def __init__(self, parent: gvsoc.systree.Component, name: str, value: int):

        super().__init__(parent, name)

        self.add_sources(['my_comp.cpp'])

        self.add_properties({
            "value": value
        })

    def i_INPUT(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'input', signature='io')

    def o_NOTIF(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('notif', itf, signature='wire<bool>')

    def i_RESULT(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'result', signature='wire<MyResult>')


class MyComp2(gvsoc.systree.Component):

    def __init__(self, parent: gvsoc.systree.Component, name: str):

        super().__init__(parent, name)

        self.add_sources(['my_comp2.cpp'])

    def i_NOTIF(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'notif', signature='wire<bool>')

    def o_RESULT(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('result', itf, signature='wire<MyResult>')