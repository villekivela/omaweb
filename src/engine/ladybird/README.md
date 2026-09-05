# Ladybird adapter

The Ladybird adapter belongs to a separately configured build and never enters the default Qt
dependency graph. Its first implementation may use bitmap painting only for functional checks;
daily-driver qualification requires a Qt Quick texture path and preservation of Ladybird's upstream
helper-process sandbox.
