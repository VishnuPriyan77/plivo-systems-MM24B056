# RUNLOG

All runs used `make && python3 run.py ...` from this directory. Overhead is the scorer's relay byte count divided by raw 160-byte frames. The final design is adjacent XOR repair plus diagonal XOR repair with a compact 1-byte relay header.

| Profile | Seed | Delay | Duration | Misses | Miss % | Overhead | Result | Change / reason |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |
| A | 1 | 40 ms | 5 s | 63/250 | 25.20% | 1.02x | INVALID | Baseline sender/receiver. Loss and jitter make pass-through unusable. |
| A | 1 | 80 ms | 10 s | 0/500 | 0.00% | 1.53x | VALID | First FEC attempt: every frame once plus one adjacent XOR packet per two frames. |
| B | 1 | 120 ms | 10 s | 4/500 | 0.80% | 1.53x | VALID | Adjacent XOR was valid but close on moderate loss/jitter. |
| B | 1 | 100 ms | 30 s | 15/1500 | 1.00% | 1.53x | VALID | Adjacent XOR only was exactly at the miss cap, so more robustness was needed. |
| B | 5 | 90 ms | 30 s | 16/1500 | 1.07% | 2.00x | INVALID | Compact header plus duplicated adjacent repair still made 90 ms too brittle. |
| A | 1 | 50 ms | 30 s | 11/1500 | 0.73% | 2.00x | VALID | Final adjacent-plus-diagonal FEC; mild profile works at low delay. |
| B | 1 | 100 ms | 30 s | 10/1500 | 0.67% | 2.00x | VALID | Final design at chosen delay. |
| B | 5 | 100 ms | 30 s | 5/1500 | 0.33% | 2.00x | VALID | Final design on a seed that was close/failing at 90 ms. |
| local burst stress | 1 | 100 ms | 30 s | 12/1500 | 0.80% | 2.00x | VALID | Temporary burst-loss profile used to check the PDF's burst-loss warning; final diagonal repair passed. |
| local burst stress | 2 | 120 ms | 30 s | 5/1500 | 0.33% | 2.00x | VALID | Same burst stress, more playout margin. |
| B | 1 | 98 ms | 30 s | 11/1500 | 0.73% | 2.00x | VALID | Final design passed the seed where piggyback FEC failed at 98 ms. |
| B | 2 | 98 ms | 30 s | 3/1500 | 0.20% | 2.00x | VALID | Final design validation sweep. |
| B | 3 | 98 ms | 30 s | 7/1500 | 0.47% | 2.00x | VALID | Final design validation sweep. |
| B | 4 | 98 ms | 30 s | 3/1500 | 0.20% | 2.00x | VALID | Final design validation sweep. |
| B | 5 | 98 ms | 30 s | 8/1500 | 0.53% | 2.00x | VALID | Final design validation sweep. |
| B | 6 | 98 ms | 30 s | 13/1500 | 0.87% | 2.00x | VALID | Final design validation sweep on a hard seed. |
| B | 7 | 98 ms | 30 s | 14/1500 | 0.93% | 2.00x | VALID | Final design validation sweep on a hard seed. |
| B | 8 | 98 ms | 30 s | 12/1500 | 0.80% | 2.00x | VALID | Final design validation sweep on a hard seed. |
| B | 6 | 97 ms | 30 s | 16/1500 | 1.07% | 2.00x | INVALID | 97 ms rejected as final target. |
| B | 7 | 97 ms | 30 s | 12/1500 | 0.80% | 2.00x | VALID | 97 ms can pass some seeds, but seed 6 failed. |
| B | 8 | 97 ms | 30 s | 11/1500 | 0.73% | 2.00x | VALID | 97 ms can pass some seeds, but seed 6 failed. |
| B | 1 | 98 ms | 30 s | 17/1500 | 1.13% | 1.99x | INVALID | Piggyback experiment: odd packets carried the previous even payload; rejected because it failed seed 1. |
| B | 3 | 90 ms | 30 s | 13/1500 | 0.87% | 2.00x | VALID | Final design can pass some 90 ms runs. |
| B | 5 | 90 ms | 30 s | 14/1500 | 0.93% | 2.00x | VALID | Still close to the cap at 90 ms. |
| B | 6 | 90 ms | 30 s | 22/1500 | 1.47% | 2.00x | INVALID | 90 ms rejected as final target. |
| B | 7 | 90 ms | 30 s | 18/1500 | 1.20% | 2.00x | INVALID | 90 ms rejected as final target. |
| B | 8 | 90 ms | 30 s | 18/1500 | 1.20% | 2.00x | INVALID | 90 ms rejected as final target. |
| B | 6 | 100 ms | 30 s | 13/1500 | 0.87% | 2.00x | VALID | Chosen delay remains valid on a hard seed. |
| B | 7 | 100 ms | 30 s | 10/1500 | 0.67% | 2.00x | VALID | Chosen delay remains valid on a hard seed. |
| B | 8 | 100 ms | 30 s | 8/1500 | 0.53% | 2.00x | VALID | Chosen delay remains valid on a hard seed. |

Decision: use `--delay_ms 98` for grading. `97 ms` and `90 ms` are competitive when they pass, but repeated full B-profile seeds crossed the 1% miss cap. The final design keeps bandwidth just under 2.0x and uses the second repair packet for a different equation rather than a duplicate, which improved clustered-loss behavior.
