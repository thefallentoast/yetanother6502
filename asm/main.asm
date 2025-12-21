.logical $8000
DBP .macro
    .byte $02
.endmacro
YES:
    LDA #$AA
    DBP
    CMP #$AA
    BEQ FINALLY
START:
    LDA #$D4
    CMP #$D3
    BEQ YES
    LDA #$FF
    DBP
    CMP #$FF
    BEQ FINALLY
FINALLY:
    LDA #$EE
    DBP
    BRK
* = $FFFC
.word START
.endlogical