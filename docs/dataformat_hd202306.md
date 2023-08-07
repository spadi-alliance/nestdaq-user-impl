# Data format at J-PARC HD 2023.06 and RCNP GR 2023.07

## Global File structure of the full configration

- Nf (Number of the FE modules) = 20
- The data bodies were skiped if the HB frame has no signal.

```
[FileSink Header]

[Filter Header (Event 0)]
[Time Frame Header (Event 0)]
[SubTime Frame Header (FE 0)]
[Data body]
[HB][HB]
[Data body]
[HB][HB]
...

[SubTime Frame Header (FE 1)]
[Data body]
[HB][HB]
[Data body]
[HB][HB]
...

[SubTime Frame Header (FE Nf)]
[Data body]
[HB][HB]
...

[Filter Header (Event Ne)]
[Time Frame Header (Event Ne)]
[SubTime Frame Header (FE 0)]
[Data body]
[HB][HB]
...
[SubTime Frame Header (FE Nf)]
[Data body]
[HB][HB]
...

[Filter Header (Event 1)]
[Time Frame Header (Event 1)]
[SubTime Frame Header (FE0)]
[Data body]
[HB][HB]
[SubTime Frame Header (FE1)]
[Data body]
[HB][HB]
...
[SubTime Frame Header (FE Nf)]
[Data body]
[HB][HB]
...
...
[FileSink Trailer]
```

## Global File structure of the three stage configration (Sampler - STFBuilder - FileSink)
```
[FileSink Header]

[SubTime Frame Header (FE N)]
[Data body]
[HB][HB]
[Data body]
[HB][HB]
...

[SubTime Frame Header (FE N)]
[Data body]
[HB][HB]
[Data body]
[HB][HB]
...
...
[FileSink Trailer]
```

## Block Headers

### File Header
||||
|:-|:-|:-
|magic            |    8B | 0x444145482d534640 "@FS-HEAD"
|size             |    8B | File Header Size
|fairMQDeviceType |    8B | --
|run Number       |    8B | Run number
|startUnixtime    |    8B | The start time of the run expressed by time_t
|stopUnixtime     |    8B | --
|comments         |  256B | Comment strings

### Filter Header | This was in here, when the filter programs were included in the run.
||||
|:-|:-|:-
|magic            |    8B | 0x4e494f43'2d544c46 "FLT-COIN"
|length           |    8B | Data length from the filter header to the end of subtime frame (Bytes)
|numTrigs         |    4B | The number of the trigger founded in the time frame
|workerId         |    4B | The id number of the filter program
|elapseTime       |   16B | Elapsed time (struct timeval)

### Time Frame Header
||||
|:-|:-|:-
|magic            |    8B | 0x444145482d465440 "@TF-HEAD"
|timeFrameId      |    4B | Id number of the time frame (the first timeFrameId of the STF)
|numSource        |    4B | The number of the source front-end modules.
|length           |    8B | Data length from the time frame header to the end of the subtime frame (Bytes)

### SubTime Frame Header
||||
|:-|:-|:-
|magic            |    8B | 0x444145482d465453 "STF-HEAD"
|timeFrameId      |    8B | Id number of the time frame
|FEMType          |    8B | Front-end module type (0: - , 1: Hi-res TDC, 2: Low-res TDC, 3: Low-res TDC version 2)
|FEMId            |    8B | Id number of the front-end module (IP address of the FEM)
|length           |    8B | The length from the subtime frame header to the end of the subtime frame (Bytes)
|numMessages      |    8B | The number of the hartbeat frame included the subtime frame
|time_sec         |    8B | The time of the subtime frame builder process the subtime frame. by struct time_val.tv_sec
|time_usec        |    8B | The same as above expressed by struct time_val.tv_usec

### File Trailer
||||
|:-|:-|:-|
magic            |   8B | 0x494152542d534640 "@FS-TRAI"
size             |   8B | File Trailer Size
fairMQDeviceType |   8B | --
runNumber        |   8B | Run number
startUnixtime    |   8B | The time of the run start expressed by time_t
stopUnixtime     |   8B | The time of the run end expressed by time_t
commensts        | 256B | Comment strings

## Data body
- High resolution TDC 64 bit format (FE type 1)
- Low resolution TDC 40 bit format (FE type 2)
- Low resolution TDC 64 bit format (FE type 3)

# Message separation
|||
|:-|:-|
| FLT Header |
| TimeFrame Header |
| SubTimeFrame Header |
| Data body |
| First HB + Second HB |
| Data body |
| First HB + Second HB |
| ... |
|SubTimeFrame Header |
| Data body |
| First HB + Second HB |
| ... |
| ... |
| FLT Header |
| TimeFrame Header |
| SubTimeFrame Header |
| ... |