import gvsoc.systree

class MyComp(gvsoc.systree.Component):
    """Tutorial component

    Dummy model which is given a value tat he will return when reading from offset 0.

    Attributes
    ----------
    parent: gvsoc.systree.Component
        The parent component where this one should be instantiated.
    name: str
        The name of the component within the parent space.
    value: int
        The value to be read
    """
    def __init__(self, parent: gvsoc.systree.Component, name: str, value: int):

        super().__init__(parent, name)

        self.add_sources(['my_comp.cpp'])

        self.add_properties({
            "value": value
        })

    def i_INPUT(self) -> gvsoc.systree.SlaveItf:
        """Returns the input port.

        Incoming requests to be handled by the component should be sent to this port.\n
        It instantiates a port of type vp::io_slave.\n

        Returns
        ----------
        gvsoc.systree.SlaveItf
            The slave interface
        """
        return gvsoc.systree.SlaveItf(self, 'input', signature='io')
