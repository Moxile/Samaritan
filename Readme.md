# Samaritan: A 4-Player Chess Engine  

## Introduction  
Welcome to Samaritan, a chess engine crafted for 4-player chess! This project is still in development, so please bear with us as we continue to enhance and polish it. Samaritan employs the Mailbox system for efficient move generation, reaching approximately 20 million nodes per second (nps, a measure of the engine's processing speed) on my hardware. I hope you have a great time exploring and using this engine!  

## Building and Running the Program  
To build and run Samaritan, execute the following commands in your terminal:  

```bash  
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release  
cmake --build build  
```  

Then, launch the executable:  
- On Windows: `.\build\samaritan.exe`  
- On Unix-like systems: `./build/samaritan`  

These instructions assume you have CMake installed and are running from the project’s root directory. The engine is built using CMake, making it compatible across different platforms.  

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