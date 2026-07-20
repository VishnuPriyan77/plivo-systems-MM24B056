# plivo-systems-MM24B056

This repository contains my solution for the Systems assignment, **The Flaky Network**. The task is to move a live stream of 160-byte frames over an unreliable UDP relay that can drop, delay, reorder, and duplicate packets. A frame is counted only if the receiver delivers the exact payload to the harness player before its playout deadline.

## Approach

The solution uses forward error correction instead of retransmission. Retransmission is expensive in this setting because both the request and the replacement packet must cross the same lossy, jittery relay, which often makes the repair arrive too late.

The sender sends each media frame once using a compact 1-byte relay header. It also sends XOR repair packets:

- adjacent XOR repair for each even/odd frame pair
- diagonal XOR repair across nearby frames for most pairs

The diagonal repair packet uses the remaining bandwidth budget to add a different recovery equation instead of sending a simple duplicate. This helps recover from clustered losses while keeping total relay bandwidth below the required `2.0x` cap.

## Receiver Design

The receiver stores received frames and repair equations in fixed-size arrays. It reconstructs the absolute frame number using `T0` and the packet's 6-bit sequence residue, then runs a small local repair pass. When an XOR equation has one known frame and one missing frame, the receiver reconstructs the missing payload and sends recovered frames to the player exactly once.

## Recommended Grading Delay

Use:

```bash
python3 run.py --profile profiles/B.json --delay_ms 98
```

The recommended playout delay is:

```text
98 ms
```

`97 ms` and `90 ms` were tested but rejected because they failed full-duration runs on harder B-profile seeds. `98 ms` passed the tested B-profile seed sweep while staying under the bandwidth limit.

## Build and Run

```bash
make
python3 run.py --profile profiles/A.json --delay_ms 98
python3 run.py --profile profiles/B.json --delay_ms 98
```

The Makefile builds:

- `./sender`
- `./receiver`

## Deliverables

This repo includes:

- `sender.c`
- `receiver.c`
- `Makefile`
- `RUNLOG.md`
- `NOTES.md`
- `SUMMARY.html`

## Notes

The project is audio-like because frames are produced every 20 ms, but the grader does not use subjective audio quality. It checks exact byte correctness, deadline misses, and relay bandwidth overhead.
