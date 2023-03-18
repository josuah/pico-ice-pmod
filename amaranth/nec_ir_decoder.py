from amaranth import *
from amaranth.sim import *


__all__ = [ "NecIrDecoder" ]


class PulseWidthDecoder(Elaboratable):
    """
    Measure the duration between two rising edges of a PWM signal.
    """
    def __init__(self, *, width=8):

        # signal input
        self.rx     = Signal()

        # data output
        self.data   = Signal(width)
        self.en     = Signal()

    def elaborate(self, platform):
        m = Module()
        last_rx = Signal(1)

        m.d.sync += last_rx.eq(self.rx)

        # on rising edge
        with m.If(~last_rx & self.rx):
            m.d.sync += self.data.eq(0)
            m.d.comb += self.en.eq(1)
        with m.Else():
            m.d.sync += self.data.eq(self.data + 1)

        return m


class NecIrDecoder(Elaboratable):
    """
    Decode NEC-style encoded IR signal coming out of an IR sensor.
    https://www.sbprojects.net/knowledge/ir/nec.php
    """

    def __init__(self, *, freq_hz):
        self.freq_hz = freq_hz

        # ir signal input
        self.rx     = Signal()

        # data output
        self.data   = Signal(32)
        self.en     = Signal()
        self.err    = Signal()

    def elaborate(self, platform):
        m = Module()
        sample_num = Signal(range(32 + 1))

        #debug = platform.request("debug", 0)
        #m.d.comb += debug.eq(self.rx)

        m.submodules.pwdec = pwdec = PulseWidthDecoder(width=32)
        m.d.comb += pwdec.rx.eq(self.rx)

        # arbitrary values
        prefix_thres_ticks = int(self.freq_hz * 9e-3)
        sample_thres_ticks = int(self.freq_hz * 1.7e-3)
        idle_thres_ticks   = int(self.freq_hz * 5e-3)


        def handle_idle_thres():
            with m.If(pwdec.data > idle_thres_ticks):
                m.d.comb += self.err.eq(1)
                m.next = "IDLE"

        m.d.sync += self.en.eq(0)

        with m.FSM() as fsm:

            with m.State("IDLE"):
                with m.If(pwdec.en & (pwdec.data > prefix_thres_ticks)):
                    m.next = "SAMPLE"

            with m.State("SAMPLE"):
                handle_idle_thres()
                with m.If(pwdec.en):
                    bit = (pwdec.data > sample_thres_ticks)
                    m.d.sync += self.data.eq(Cat(bit, self.data))
                    m.d.sync += sample_num.eq(sample_num + 1)
                with m.If(sample_num == 32):
                    m.d.sync += sample_num.eq(0)
                    m.d.sync += self.en.eq(1)
                    m.next = "IDLE"

        return m

if __name__ == "__main__":
    nul = "                                                     "
    pfx = "################        "
    pfx += "# # # # # # # # #   #   #   #   #   #   # #   "

    rx = nul
    rx += pfx + "# # #   #   # # # # #   #   # # #   #   #   #   #" + nul # Num0
    rx += pfx + "# # # # #   # # # #   #   #   #   # #   #   #   #" + nul # Num1
    rx += pfx + "#   # # # #   # # # # #   #   #   # #   #   #   #" + nul # Num2
    rx += nul

    freq_hz = 2e4 # MHz

    dut = NecIrDecoder(freq_hz=freq_hz)

    def bench():
        for ch in rx:
            for _ in range(int(freq_hz * 562.5e-6)):
                yield
            yield dut.rx.eq(ch == "#")

    sim = Simulator(dut)
    sim.add_clock(1/freq_hz)
    sim.add_sync_process(bench)
    with sim.write_vcd(vcd_file=f"{__file__[:-3]}.vcd"):
        sim.run()