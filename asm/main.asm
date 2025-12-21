.logical $8000
DBP .macro
    .byte $02
.endmacro
START:
    LDA #$FF
    DBP
    STA $0100
    LDA #$00
    DBP
    LDA $0100
    DBP
    BRK
* = $FFFC
.word START
.endlogical