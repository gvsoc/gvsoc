import cpu.iss.riscv
import memory.memory
import vp.clock_domain
import interco.router
import utils.loader.loader
import gvsoc.systree
import gvsoc.runner
import my_comp


GAPY_TARGET = True



class MyRiscv(cpu.iss.riscv.RiscvCommon):
    def __init__(self,
            parent: gvsoc.systree.Component, name: str, isa: str='rv64imafdc', binaries: list=[],
            fetch_enable: bool=False, boot_addr: int=0, timed: bool=True,
            core_id: int=0):

        # Instantiates the ISA from the provided string.
        isa_instance = cpu.iss.isa_gen.isa_riscv_gen.RiscvIsa(isa, isa)

        for insn in isa_instance.get_insns():
            if insn.label == "my_instr":
                insn.get_out_reg(0).set_latency(100)

        # And instantiate common class with default parameters
        super().__init__(parent, name, isa=isa_instance, misa=0,
            riscv_exceptions=True, riscv_dbg_unit=True, binaries=binaries, mmu=True, pmp=True,
            fetch_enable=fetch_enable, boot_addr=boot_addr, internal_atomics=True,
            supervisor=True, user=True, timed=timed, prefetcher_size=64, core_id=core_id)

        self.add_c_flags([
            "-DPIPELINE_STAGES=2",
            "-DCONFIG_ISS_CORE=riscv",
        ])



class Soc(gvsoc.systree.Component):

    def __init__(self, parent, name, parser):
        super().__init__(parent, name)

        # Parse the arguments to get the path to the binary to be loaded
        [args, __] = parser.parse_known_args()

        binary = args.binary

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
        host = MyRiscv(self, 'host', isa='rv64imafdc')
        host.o_FETCH     ( ico.i_INPUT     ())
        host.o_DATA      ( ico.i_INPUT     ())

        # Finally connect an ELF loader, which will execute first and will then
        # send to the core the boot address and notify him he can start
        loader = utils.loader.loader.ElfLoader(self, 'loader', binary=binary)
        loader.o_OUT     ( ico.i_INPUT     ())
        loader.o_START   ( host.i_FETCHEN  ())
        loader.o_ENTRY   ( host.i_ENTRY    ())



# This is a wrapping component of the real one in order to connect a clock generator to it
# so that it automatically propagate to other components
class Rv64(gvsoc.systree.Component):

    def __init__(self, parent, name, parser, options):

        super().__init__(parent, name, options=options)

        clock = vp.clock_domain.Clock_domain(self, 'clock', frequency=100000000)
        soc = Soc(self, 'soc', parser)
        clock.o_CLOCK    ( soc.i_CLOCK     ())




# This is the top target that gapy will instantiate
class Target(gvsoc.runner.Target):

    def __init__(self, parser, options):
        super(Target, self).__init__(parser, options,
            model=Rv64, description="RV64 virtual board")

