# LogicFilter

## Overview
LogicFilter processes input TDC (Time-to-Digital Converter) data based on a user-defined logical expression. If the expression evaluates to "true" at a specific timestamp, that timestamp is added to the output data.
The processing time unit is four times the unit of the input TDC data. For example, if the input TDC data unit is 1 ns, the output data unit will be 4 ns.

## Redis DB Keys

### Key prefix
- Defaut: parameter:LogicFilger:

### trigger-signals
Defines the input channels. Each input signal is specified as a set: (<group id> <module id> <channel id> <offset> <left width> <right width>). The 4th and 5th paramter are optional.(detail in the section "trigger-width")
Signal IDs: Assigned sequentially starting from 0 based on the order of the definitions.
Capacity: A maximum of 32 signals can be defined.
- Default: (0xc0a802a9 0 0) (0xc0a802a9 1 0)

### trigger-expression
Defines the logical expression applied to the input signals. It supports both Infix notation and RPN (Reverse Polish Notation).

**Operands:**
- Numbers represent the assigned Signal IDs.

**Operators:**
- & : Logical AND
- | : Logical OR
- ! : Logical NOT

**Syntax:**
- Infix: Use parentheses ( and ) for order of operations.
- RPN: Must be prefixed with the keyword RPN.

- Default: RPN 0 1 &  
(Generates a trigger based on the coincidence of input channels 0 and 1.)

### trigger-width
Sets the TDC coincidence window. Let $T$ be the trigger-width value and $t$ be the TDC value; the window is defined as: 
$$[t - T/2, t + T/2]$$
When user gives trigger-signals with 3 parameters(, which are <module id>, <channel id>, and <offset>), the TDC coincidence window is defined as above.
When user gives trigger-signals with 5 parameters(, which are <module id>, <channel id>, <offset>, <left width>, and <right width>), the TDC coincidence window is defined as $$[t - leftwidth, t+ rightwidth]$$
The time unit for $T$ is four times the time unit of the input TDC data.
- Default: 10

# Still in Experimental
## Extended capacity
We can now support trigger logic with more than 32 channels.

## trigger-signals
Defines the input channels. Each input signal is specified as a set: (<Group-Subgroup id> <module id> <channel id> <offset>).
Group IDs: Defines which group and subgroup the signal belongs to.( "Group", and "Subgroup" are described below. )
Capacity: Up to 32 signals can be registered to each Subgroup. Up to 32 groups can be defined.
- Default: `(0 0xc0a802a9 0 0) (1 0xc0a802a9 1 0)`

### Group and Subgroup
A **Group** represents one logical unit used in `trigger-expression`. A Group can contain one or more signals and **Subgroups**. A **Subgroup** can contain at least two signals.
- Signals in the same Subgroup are combined with logical AND.
- Signals and Subgroups in the same Group are combined with logical OR.

## trigger-expression
Defines the logical expression applied to the groups. It supports both Infix notation and RPN (Reverse Polish Notation).
- Default: `0 & 1`

### Compatible expressions
With `trigger-signals` and `trigger-expression`, 
```(0 fem0 ch0 offs0) (1 fem1 ch1 offs1) (2-0 fem2 ch2 offs2) (2-0 fem3 ch3 offs3) (2-0 fem4 ch4 offs4) (2 fem5 ch5 offs5) (2 fem6 ch6 offs6)```,
```(0 & 1) & 2```, it has a expression, being equivalent. That is
```(0 fem0 ch0 offs0) (1 fem1 ch1 offs1) (2 fem2 ch2 offs2) (3 fem3 ch3 offs3) (4 fem4 ch4 offs4) (5 fem5 ch5 offs5) (6 fem6 ch6 offs6)```,
```(0 & 1) & ((2 & 3 & 4) | (5 & 6))```.

### Drawback of using Subgroups
Computational cost depends on number of Subgroups. If your trigger logic has less than 32 channels, it is recommended to use without subgroups.



- 32ch以上のトリガーロジックへの対応( https://github.com/SATSUKIUM/nestdaq-user-impl/tree/dev_lf_more32_debug )
- クローンしてdev_lf_more32_debugのブランチをプルすると使える
- TriggerLogic.shの記法が変更された
- いままで: signalの組み合わせがtrigger-expressionと呼ばれていた
- これから: signalを1つ以上持つgroupの組み合わせをtrigger-expressionと呼ぶ。
- つまり、シグナルをグループでラップすることで実質的にLUTに32ch以上を詰め込むことがこのアップデートである
### 新しい記法
- "trigger-signals"は(group IP CH Offset left_width right_width)もしくは、(“Group-Subgroup” IP CH Offset left_width right_width)を列挙することになります
- 同じグループに属するチャンネル/サブグループはORのロジックが自動で組まれます
- 同じサブグループに属するチャンネルはANDのロジックが自動で組まれます
- "trigger-expression"はこれまでどおりの記法だが、グループ同士のロジックを記述する