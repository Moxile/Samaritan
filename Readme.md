# Samaritan: A 4-Player Chess Engine  

## Introduction  
Welcome to Samaritan, a chess engine crafted for 4-player chess! This project is still in development, so please bear with us as we continue to enhance and polish it. Samaritan employs the Mailbox system for efficient move generation, reaching approximately 20 million nodes per second (nps, a measure of the engine's processing speed) on my hardware. I hope you have a great time exploring and using this engine!  

## Building and Running the Program  

Samaritan builds with CMake on Linux, macOS and Windows. The only required
dependency is [CLI11](https://github.com/CLIUtils/CLI11); GoogleTest (unit
tests) and Google Benchmark (`samaritan --perft`) are used when present and
skipped when not.

### Linux / macOS

```bash
cd samaritan
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/samaritan
```

### Windows

The dependencies come from [vcpkg](https://vcpkg.io); MSVC 2022 (17.6+) or a
recent clang-cl is needed for the C++23 features the engine uses.

```powershell
vcpkg install cli11 gtest benchmark
cd samaritan
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
.\build\Release\samaritan.exe
```

MSYS2/MinGW works too, with the same commands as the Unix build.

### Build options

| option | default | meaning |
|---|---|---|
| `SAMARITAN_NATIVE` | `ON` | tune for this machine (`-march=native`, or `/arch:AVX2` on MSVC). Turn it **off** for a binary that has to run on other CPUs. |
| `SAMARITAN_TOOLS` | `ON` | build the `bench/` tools and register the correctness oracles with CTest. |
| `SAMARITAN_INTEGRATION_TESTS` | `OFF` | also register the gates that need the Python trainer (feature and forward parity). |

### Testing

```bash
ctest --test-dir build --output-on-failure
```

That runs the unit tests plus the bounded correctness oracles: whole-state
invariants after every make/undo, check and pin metadata against a brute-force
scan, differential perft against the recorded reference counts, and the
incremental NNUE accumulators against a full refresh. Individual groups:
`ctest -L unit`, `ctest -L oracle`.

The `bench/` tools are built by `cmake --build build --target tools` and land in
`build/bench/`; run them directly for deeper walks, e.g.
`./build/bench/verify bench/positions.tsv bench/ref_depth5.tsv 5`.

### Using the engine

```
uci
setoption name hash value 128
setoption name evalfile value path/to/net.snn1   # optional: NNUE weights
position modern moves e2e4 b11c11
go depth 6
```

`position` accepts `modern`, `classic`, `startpos` or `fen <FEN>`, each
optionally followed by `moves <move> ...`. Moves are file+rank pairs over the
14x14 board (`a1`..`n14`), with an optional promotion suffix (`j13j14q`). Without a network the engine
evaluates material, which is deliberately its permanent baseline.

## Board Indices  
The chessboard in Samaritan is represented as a grid with indices ranging from 0 to 223, as shown below. Each number corresponds to a specific square on the 4-player chessboard, which is crucial for understanding how moves are generated and positions are evaluated. For a standard 4-player chess setup, the board is typically a 14x14 grid (196 squares), though the engine uses a larger 16x14 array internally, with some indices possibly reserved for padding or special purposes. The vertical bars in the grid highlight the main playing area.  

```
  0   1   2   3|  4   5   6   7   8   9  10  11| 12  13  14  15  
 16  17  18  19| 20  21  22  23  24  25  26  27| 28  29  30  31  
 32  33  34  35| 36  37  38  39  40  41  42  43| 44  45  46  47  
 48| 49  50  51  52  53  54  55  56  57  58  59  60  61  62| 63  
 64| 65  66  67  68  69  70  71  72  73  74  75  76  77  78| 79  
 80| 81  82  83  84  85  86  87  88  89  90  91  92  93  94| 95  
 96| 97  98  99 100 101 102 103 104 105 106 107 108 109 110|111  
112|113 114 115 116 117 118 119 120 121 122 123 124 125 126|127  
128|129 130 131 132 133 134 135 136 137 138 139 140 141 142|143  
144|145 146 147 148 149 150 151 152 153 154 155 156 157 158|159  
160|161 162 163 164 165 166 167 168 169 170 171 172 173 174|175  
176 177 178 179|180 181 182 183 184 185 186 187|188 189 190 191  
192 193 194 195|196 197 198 199 200 201 202 203|204 205 206 207  
208 209 210 211|212 213 214 215 216 217 218 219|220 221 222 223  
```

## Credits  
- **Special Thanks** to [Kooser](https://github.com/kooser6), also known as [Machina](https://www.chess.com/member/teammachina1), for introducing me to the world of chess programming and patiently answering my many questions. Without their guidance, this project wouldn’t exist.  
- **Special Thanks** to [Ariana](https://github.com/arianahejazyan) for inspiring me to keep pushing forward and finish this project, as well as for countless hours of coding together.  
- **Thanks** to TeamTitan and TeamTerminator for their quick and helpful responses to my questions.