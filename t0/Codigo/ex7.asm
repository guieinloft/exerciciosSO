TELA DEFINE 2
TELAOK DEFINE 3

GERADOR DEFINE 18 ; define gerador

     cargm dez
laco trax
     le GERADOR
     resto cem
     chama escint
     trax
     sub um
     desvp laco
     para



; função para escrever um char (em A) na tela
; retirada do exercicio 5
escch   espaco 1
        armm ec_tmp
ec_1
        le TELAOK
        desvz ec_1
        cargm ec_tmp
        escr TELA
        ret escch
ec_tmp  espaco 1

; escreve o valor de A no terminal, em decimal
; retirada do exercicio 5
escint  espaco 1
        ; ei_num = A
        armm ei_num
        ; if ei_num > 0 goto ei_pos
        desvp ei_pos
        ; if ei_num < 0 goto ei_neg
        desvn ei_neg
        ; print '0'; goto ei_f
        cargm a_zero
        chama escch
        desv ei_f
ei_neg
        ; escnum = -escnum
        neg
        armm ei_num
        ; print '-'
        cargm a_menos
        chama escch
ei_pos
        ; faz ei_mul ser a maior potência de 10 <= ei_num
        ; ei_mul = 1
        cargi 1
        armm ei_mul
ei_1
        ; if ei_mul == ei_num goto ei_3
        cargm ei_mul
        sub ei_num
        desvz ei_3
        ; if ei_mul > ei_num goto ei_2
        desvp ei_2
        ; ei_mul *= 10
        cargm ei_mul
        mult dez
        armm ei_mul
        ; goto ei_1
        desv ei_1
ei_2
        ; ei_mul /= 10
        cargm ei_mul
        div dez
        armm ei_mul
ei_3
        ; print (ei_num/ei_mul) % 10 + '0'
        cargm ei_num
        div ei_mul
        resto dez
        soma a_zero
        chama escch
        ; ei_mul /= 10
        cargm ei_mul
        div dez
        armm ei_mul
        ; if ei_mul > 0 goto ei_3
        desvp ei_3
ei_f
        ; print ' '
        cargi ' '
        chama escch
        ; return
        ret escint
ei_num  espaco 1
ei_mul  espaco 1

; constantes
um valor 1
dez valor 10
nove valor 9
cem valor 100
a_zero valor '0'
a_menos valor '-'
