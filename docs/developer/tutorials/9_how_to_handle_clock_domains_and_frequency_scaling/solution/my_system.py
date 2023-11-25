import gvsoc.systree
import gvsoc.runner

import cpu.iss.riscv
import memory.memory
import vp.clock_domain
import interco.router
import utils.loader.loader
import gdbserver.gdbserver

import my_comp


GAPY_TARGET = True

class Rv64(gvsoc.systree.Component):

    def __init__(self, parent, name, parser, options):

        super().__init__(parent, name, options=options)

        # Parse the arguments to get the path to the binary to be loaded
        [args, __] = parser.parse_known_args()

        binary = args.binary

        clock1 = vp.clock_domain.Clock_domain(self, 'clock1', frequency=100000000)
        clock2 = vp.clock_domain.Clock_domain(self, 'clock2', frequency=100000000)

        # Main interconnect
        ico = interco.router.Router(self, 'ico')

        # Custom components
        comp = my_comp.MyComp(self, 'my_comp', value=0x12345678)
        ico.o_MAP(comp.i_INPUT(), 'comp', base=0x20000000, size=0x00001000, rm_base=True)

        # Main memory
        mem = memory.memory.Memory(self, 'mem', size=0x00100000)
        # The memory needs to be connected with a mpping. The rm_base is used to substract
        # the global address to the requests address so that the memory only gets a local offset.
        ico.o_MAP(mem.i_INPUT(), 'mem', base=0x00000000, size=0x00100000, rm_base=True)

        # Instantiates the main core and connect fetch and data to the interconnect
        host = cpu.iss.riscv.Riscv(self, 'host', isa='rv64imafdc')
        host.o_FETCH     ( ico.i_INPUT     ())
        host.o_DATA      ( ico.i_INPUT     ())

        # Finally connect an ELF loader, which will execute first and will then
        # send to the core the boot address and notify him he can start
        loader = utils.loader.loader.ElfLoader(self, 'loader', binary=binary)
        loader.o_OUT     ( ico.i_INPUT     ())
        loader.o_START   ( host.i_FETCHEN  ())
        loader.o_ENTRY   ( host.i_ENTRY    ())

        clock1.o_CLOCK    ( host.i_CLOCK     ())
        clock2.o_CLOCK    ( ico.i_CLOCK     ())
        clock2.o_CLOCK    ( mem.i_CLOCK     ())
        clock2.o_CLOCK    ( comp.i_CLOCK     ())
        clock2.o_CLOCK    ( loader.i_CLOCK     ())

        comp.o_CLK_CTRL   (clock1.i_CTRL())


# This is the top target that gapy will instantiate
class Target(gvsoc.runner.Target):

    def __init__(self, parser, options):
        super(Target, self).__init__(parser, options,
            model=Rv64, description="RV64 virtual board")

