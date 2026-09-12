; Fill the following columns for each batch run.  The first line of data should
; contain the starting simulation number.  Output filenames will be xxx.sim
; where xxx is the simulation number.  Comments must begin in the first column
; and must be preceded with a semicolon.  Separate each column of data with one
; or more spaces.  Indicate the end of a batch with an asterix.
; Fore 'env type' 0 = standard, 1 = concave.
;
;
; Starting simulation number:
1
; Simulation information:
;
;     | num  |env |BBA|soft |CDO|CEO|elite|elite |Head|max  |num  |msg to|gen|bid  |init |stren|bid  |xover|mutate|goal  |dir   |obst  |crash|goal   |obst |sensor|time |agent|net|class|class |class |class |Tx stren|Rx stren|act |act   |act   |act   |Tx bid|Rx bid|
; seed|agents|type|on |reset|on |on | on  |thresh|tax |count|class| post |int|const|stren|cap  |cap  |prob |prob  |reward|reward|reward|pen  |range  |range|range |const|dia  |on |pass |Tx int|max Tx|max Rx|thresh  |thresh  |pass|Tx int|max Tx|max Rx|thresh|thresh|
  100    2     0    Y    N    Y   Y    N    0.0   0.0  4000   32      1   20  .125   0.0  100.0 100.0  .7    .1     2.0    5.0    0.0    5.0   2000.0  50.0  1.57   15.0  15.0   Y   Y      1      1      1      0.8      0.8     N     1      1      1     0.8    0.8
; End of batch file must have an * in the first column:
*

