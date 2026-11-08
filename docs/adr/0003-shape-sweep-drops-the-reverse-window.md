# The Shape sweep drops the reverse window

Stock Beads sweeps Shape from square through decay and bell to reverse, so the softest window sits about two-thirds of the way round and there is no soft end. We remap Shape, anticlockwise to clockwise, as one continuous morph: decay, square, Tukey, Hann, Gaussian. The diffuser fades in from Hann to fully clockwise. Reverse is dropped because it is the least soft shape, and keeping it would break the morph. Decay and square stay for percussive use. The morph stays continuous, like Arbhar and stock Beads, rather than stepping between discrete shapes like Multigrain, so knob moves and CV never jump between grains.

## Considered options

- **Keep the stock layout and only swap the bell for a Gaussian:** this leaves the diffuser with no natural place on the knob.
- **Make every position smooth, as the original brief said:** this loses decay and square.
- **Discrete shapes, as Multigrain does (including sinc and triangle):** shape changes would jump.
