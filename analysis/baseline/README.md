# MISRA baseline

`misra-baseline.json` records the findings accepted when the analysis was adopted. CI runs `misch run --baseline` and fails on any finding not recorded there. Regenerate it only after a deliberate review, with `misch baseline`; do not edit it by hand.
