# Audit the engine flags Omaweb adds itself

Omaweb writes flags of its own into `QTWEBENGINE_CHROMIUM_FLAGS` before the engine starts, and they
go through the audit in `readDevelopmentLaunch` that flags from the command line and from the
environment go through. There is one statement of what a launch may carry, not one per route in.

Until this, the audit read a command line Omaweb only inspected. Its refusals are the reason the
sandbox cannot be turned off from the environment
([ADR 0013](0013-preserve-engine-sandboxes-in-every-build.md)), and a flag Omaweb added itself would
have passed that audit by never reaching it. A second route into the engine's command line with no
rule on it is worth more to an attacker who can influence a build than the flags it was opened for.
So the audit reads the line the engine will read, and Omaweb's own flags are in it.

Omaweb's flags go last on that line, after the environment's. Chromium reads the last occurrence of
a switch, so the last `--enable-features` list is the effective one, and Omaweb's carries forward
whatever the environment already named. A reader who had to name a companion feature to get their
VA-API driver working keeps hardware decode rather than losing it to the naming. The cost of this
order is that a reader cannot overrule an Omaweb flag by naming the same switch. What they can do
instead is say no to the thing itself, which for hardware decode is
`--disable-features=VaapiVideoDecodeLinuxGL` or turning the GPU process off, and Omaweb then adds
nothing. That is a narrower control than last-wins ordering and a clearer one: it names the
behaviour being refused rather than a switch position.

The alternative was to place Omaweb's flags first and let the environment overrule them. It reads as
the more deferential order and is not: a reader naming any feature list would have silently replaced
Omaweb's, so asking for one unrelated feature would have cost hardware decode with nothing saying
so.

What Omaweb may add itself is bounded by the audit, which refuses a sandbox switch, a web security
boundary, and a debugging listener whatever route they arrive by. Beyond that, an added flag has to
be one a host without the hardware to serve it can ignore: the engine falls back and starts, rather
than the launch failing. Hardware video decode is the first such flag.
