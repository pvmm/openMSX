; MSX1 16 KiB cartridge ROM
; Simple CALL/RET for testing step_back_over
        ORG     $4000

        DEFB    "AB"
        DEFW    INIT
        DEFW    0
        DEFW    0
        DEFW    0

INIT:   DI
        LD      BC,$1234
        CALL    SUB1
        LD      DE,$5678        ; return point after first call
        JR      FOREVER

SUB1:   LD      HL,$ABCD
        RET

FOREVER:
        JR      FOREVER

        DS      $8000-$, $FF
