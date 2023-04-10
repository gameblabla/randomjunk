;------------------------------------------------------------------------------
;
;	wss DMAモードでwav/aiff再生
;
;------------------------------------------------------------------------------
ver	equ	"0.3"
date	equ	"1998/11/13"
bufsize	equ	24*1024
code	segment
	assume	cs:code,ds:code,es:code,ss:code
	org	100h
start:
	cld
	mov	ax,cs
	mov	ds,ax
	mov	es,ax

	mov	dx,offset title_mes
	mov	ah,9
	int	21h

;------------------------------------------------------------------------------
;	ワーク初期化
;------------------------------------------------------------------------------
	xor	ax,ax
	mov	di,offset initarea
	mov	cx,offset initend - offset initarea
rep	stosb

;------------------------------------------------------------------------------
;	バッファ1,2アドレス計算 (DMA設定用)
;------------------------------------------------------------------------------
	mov	ax,cs
	xor	dx,dx	;DX:AX = CS

	add	ax,ax	;4 bit shift
	adc	dx,dx
	add	ax,ax
	adc	dx,dx
	add	ax,ax
	adc	dx,dx
	add	ax,ax
	adc	dx,dx	;DX:AX= real address of CS:0000

	add	ax,offset buffer1
	adc	dx,0
	mov	[buffer1ofs],ax
	mov	[buffer1ofs2],dl
	add	ax,bufsize
	adc	dx,0
	mov	[buffer2ofs],ax
	mov	[buffer2ofs2],dl

;------------------------------------------------------------------------------
;	コマンドラインからオプション、ファイル名の取り込み
;------------------------------------------------------------------------------
	mov	si,offset 81h
loop1:	lodsb
	cmp	al," "
	jc	usage
	jz	loop1
	dec	si

	cmp	al,"-"
	jz	get_opt
	cmp	al,"/"
	jnz	get_fn
get_opt:
	inc	si
	lodsb
	and	al,11011111b		;小文字＞大文字変換
	cmp	al,"V"
	jz	opt_v
	cmp	al,"S"
	jz	opt_s
	cmp	al,"F"
	jz	opt_f
	cmp	al,"T"
	jz	opt_t
	cmp	al,"D"
	jz	opt_d
	cmp	al,"I"
	jnz	usage
opt_i:
	call	get_num
	jc	usage
	mov	[def_int],al
	cmp	al,2
	jc	loop1	;0,1
	cmp	al,4
	jc	usage	;2,3
	cmp	al,41
	jz	oi_00	;41
	cmp	al,6
	jnc	usage	;6...255
	sub	[def_int],2	;4,5
	jmp	loop1
oi_00:	mov	[def_int],2
	jmp	loop1
opt_d:
	call	get_num
	jc	usage
	mov	[def_dma],al
	cmp	al,2
	jc	loop1
	cmp	al,3
	jnz	usage
	dec	[def_dma]
	jmp	loop1
opt_t:
	call	get_num
	jc	usage
	cmp	al,7
	jnc	usage
	mov	[def_type],al
	jmp	loop1
opt_f:
	call	get_num
	jc	usage
	mov	[def_rate],bx
	jmp	loop1
opt_s:
	mov	[def_ch],2
	jmp	loop1
opt_v:
	call	get_num
	jc	usage
	cmp	al,64
	jnc	usage
	mov	ah,63
	sub	ah,al
	mov	[volume],ah
	jmp	loop1
get_fn:
	mov	di,offset filename
	mov	bx,offset wav_ext
	call	get_filename

;------------------------------------------------------------------------------
;	ファイル読み込み
;------------------------------------------------------------------------------
	mov	dx,offset filename
	mov	ax,3d00h
	int	21h		;open
	jc	fileerr
	mov	[fhand],ax

;------------------------------------------------------------------------------
;	ファイル種別確認
;------------------------------------------------------------------------------
	call	get_pcminfo

	call	read_buffer

;------------------------------------------------------------------------------
;	wss存在確認
;------------------------------------------------------------------------------

;	サウンド識別IDポートの取得
	mov	dx,0c24h
	mov	al,0e1h
	out	dx,al
	mov	dx,0c2bh
	in	al,dx
	mov	ah,al
	mov	dx,0c2dh
	in	al,dx
	xchg	ah,al

;	ID判別
	mov	dx,ax		;get sound ID (def=a460h)
	in	al,dx
	and	al,0f0h
	cmp	al,01100000b
	jc	no_wss
	jz	wss_np
	cmp	al,11110000b
	jc	wss_found
	jmp	no_wss

;	PC-9821Npの場合,0f40hポートのINTを初期化
wss_np:
	mov	dx,881eh
	in	al,dx
	and	al,00011000b
	add	al,00001000b
	mov	ah,al
	mov	dx,0f40h
	in	al,dx
	and	al,11000111b
	or	al,ah
	out	dx,al

wss_found:
;------------------------------------------------------------------------------
;	DMA/Int設定
;------------------------------------------------------------------------------
	mov	dx,0f40h
	in	al,dx

	cmp	[def_dma],-1
	jz	no_set_dma
	mov	ah,[def_dma]
	inc	ah
	and	al,11111000b
	or	al,ah
no_set_dma:
	cmp	[def_int],-1
	jz	no_set_int
	mov	ah,[def_int]
	inc	ah
	add	ah,ah
	add	ah,ah
	add	ah,ah
	and	al,11000111b
	or	al,ah
no_set_int:
	out	dx,al

	mov	ah,al
	and	ax,0000011100111000b
	shr	al,1
	shr	al,1
	shr	al,1		;ah=DMA al=Int.
	jz	wss_err
	test	ah,ah
	jz	wss_err

;	int.設定
	dec	al
	xor	bx,bx
	mov	dx,bx
	mov	bl,al
	add	bx,bx
	add	bx,bx
	mov	dl,[int_table +bx]
	mov	[int_vec],dl
	mov	dl,[int_table +bx +1]
	mov	[int_port],dx
	mov	dl,[int_table +bx +2]
	mov	[int_mask],dl

;	DMA port 設定
	dec	ah
	xor	bx,bx
	mov	dx,bx
	mov	bl,ah
	add	bx,bx
	add	bx,bx
	mov	dl,[dma_table +bx]
	mov	[dmanum],dl
	mov	dl,[dma_table +bx +1]
	mov	[dma_bank],dx
	mov	dl,[dma_table +bx +2]
	mov	[dma_adr],dx
	mov	dl,[dma_table +bx +3]
	mov	[dma_count],dx

;------------------------------------------------------------------------------
;	Int. vector init
;------------------------------------------------------------------------------
	cli			;割り込み禁止

	mov	ah,35h
	mov	al,[int_vec]
	int	21h		;get vector
	mov	[intvec_seg],es
	mov	[intvec_ofs],bx	;保存

	mov	dx,offset dmaint
	mov	ah,25h
	mov	al,[int_vec]
	int	21h		;set vector

	mov	ax,cs
	mov	es,ax

	mov	dx,[int_port]
	in	al,dx
	mov	ah,[int_mask]
	not	ah
	and	al,ah
	out	dx,al		;Int Enable

;------------------------------------------------------------------------------
;	CS4231 Init.
;------------------------------------------------------------------------------

;	mode flag set
	mov	ax,0cc0h	;mode CS4231
	call	wssset
;	DAC/HPF set
	mov	ax,1080h	;DAC on/Timer off/0dB MODE
	call	wssset
	mov	ax,1100h	;HPF off
	call	wssset
;	Volume set
	mov	al,[volume]
	mov	ah,06		;Left DAC
	call	wssset
	mov	ah,07		;Right DAC
	call	wssset

;	DMA base register set
	mov	bx,bufsize-1
	cmp	[adpcm_flag],1
	jnz	no_ad
	shr	bx,1		;ADPCMなら /4
	shr	bx,1
	jmp	set_br
no_ad:	cmp	[stereo_flag],1
	jnz	no_st
	shr	bx,1		;STEREOなら /2
no_st:	cmp	[bit16_flag],1
	jnz	set_br
	shr	bx,1		;16bitなら /2

set_br:	mov	ah,0eh		;upper
	mov	al,bh
	call	wssset
	mov	ah,0fh		;lower
	mov	al,bl
	call	wssset

;	Format/Rate set
	mov	ah,48h		;format/ch/rate
	mov	al,[pcm_form_rate]	
	call	wssset		;１回目
	call	wsswait		;INIT待ち
	call	wssset		;２回目
	call	wsswait		;INIT待ち

;	音量表示用にOutput -> Input Loopback 設定

;	ADC Input
	mov	ax,00c0h
	call	wssset		;Left Input Line =Output Loopback 0dB
	mov	ax,01c0h
	call	wssset		;Right //
;	LoopBack Control
	mov	ax,0d00h	;Loopback無効
	call	wssset
;	Format/Rate set
	mov	ax,5c10h	;format/ch/rate(Input,8bit/Linear/Stereo PCM)
	call	wssset		;１回目
	call	wsswait		;INIT待ち
	call	wssset		;２回目
	call	wsswait		;INIT待ち

;	Int.flag Reset
	mov	dx,0f46h
	out	dx,al		;Int.Reset
;	Int.Enable
	mov	ax,0a02h	;Int.Enable
	call	wssset

	call	dma_program

;------------------------------------------------------------------------------
;	Play Start
;------------------------------------------------------------------------------
;	mov	ax,4901h	;DMA mode/ Play (MCE bit ON)
	mov	ax,4987h	;Play=DMA mode Rec=PIO mode dual (MCE bit ON)
	call	wssset

	mov	dx,0f44h
	mov	al,20h
	out	dx,al		;MCE off

	sti			;割り込み開始

;------------------------------------------------------------------------------
;	Play Main
;------------------------------------------------------------------------------
main_loop1:
	cmp	[fileend_flag],1	;最後のデータを読んだか？
	jnz	main_read		;読んでなければ読む
	inc	[fileend_flag]
	jmp	main_loop2		;読んでれば、読まずにもう１回待つ
main_read:
	call	read_buffer		;データ読み込み
main_loop2:
	call	put_vol
	mov	bx,es
	xor	ax,ax
	mov	es,ax
	test	byte ptr es:[052ah],00000001b	;ESCが押されたら
	mov	es,bx
	jnz	mainexit			;終了

	cmp	[filenext_flag],0	;次のデータ要求が来たか？
	jz	main_loop2		;くるまで待つ
	mov	[filenext_flag],0	;データ要求フラグクリア

	cmp	[fileend_flag],2	;最後のデータを読んだあと
	jc	main_loop1		;１回転送していれば終了

mainexit:
;------------------------------------------------------------------------------
;	再生停止
;------------------------------------------------------------------------------
	cli

;	Play Stop
	mov	ax,4900h	;Play STOP (MCE bit ON)
	call	wssset
;	Int.Disable
	mov	ax,0a00h	;Int.Disable
	call	wssset

;------------------------------------------------------------------------------
;	割り込みを元に戻す
;------------------------------------------------------------------------------
	mov	ah,25h
	mov	al,[int_vec]
	lds	dx,dword ptr [intvec_seg]
	int	21h		;set vector

	mov	ax,cs
	mov	ds,ax

	mov	dx,[int_port]
	in	al,dx
	or	al,[int_mask]
	out	dx,al		;Int Disable

	sti

;------------------------------------------------------------------------------
;	終了
;------------------------------------------------------------------------------
	mov	bx,[fhand]
	mov	ah,3eh
	int	21h		;close

	mov	dx,offset fin_mes
exit:	mov	ah,9
	int	21h		;print

	mov	ax,4c00h
	int	21h

;------------------------------------------------------------------------------
;	エラー処理
;------------------------------------------------------------------------------
no_wss:
	mov	dx,offset nowss_mes
	jmp	exit
fileerr:
	mov	dx,offset fileerr_mes
	jmp	exit
wss_err:
	mov	dx,offset wsserr_mes
	jmp	exit
form_err:
	mov	dx,offset formerr_mes
	jmp	exit
usage:
	mov	dx,offset usage_mes
	jmp	exit

;------------------------------------------------------------------------------
;	音量表示
;------------------------------------------------------------------------------
put_vol:
	mov	dx,0f46h
	in	al,dx
	test	al,00100000b	;CRDY
	jz	pv_exit

	mov	ax,60h
	mov	ds,ax
	mov	dx,ds:[0111h]
	mov	ax,cs
	mov	ds,ax
	test	dl,dl		;ファンクション未表示状態では
	jz	pv_exit		;表示しない
	mov	dl,dh
	xor	dh,dh
	inc	dx		;dx=最終行数

	add	dx,dx
	add	dx,dx
	add	dx,dx
	add	dx,dx
	add	dx,dx
	mov	ax,dx
	add	dx,dx
	add	dx,dx
	add	dx,ax		;dx=dx*160 [表示位置]
	mov	di,dx
	add	di,2
	mov	dx,0f46h
	in	al,dx
	test	al,01000000b	;CU/R
	jz	pv_right
	call	put_vol_sub
	ret
pv_right:
	add	di,76*2
	call	put_vol_sub
pv_exit:
	ret

put_vol_sub:
	mov	dx,0f47h
	in	al,dx
	sub	al,80h
	jnc	pv_01
	neg	al
	jns	pv_01
	dec	al
pv_01:
	mov	ah,al
	and	ax,0111100000000100b
	shr	ah,1
	shr	ah,1
	shr	ah,1
	jnz	pv_02
	test	al,al
	mov	al," "
	jz	pv_03
	mov	al,80h
	jmp	pv_03
pv_02:	mov	al,ah
	or	al,80h
	test	al,08h
	jz	pv_03
	mov	al,87h
pv_03:	mov	bx,0a000h
	mov	ds,bx
	mov	byte ptr [di],al
	mov	byte ptr [di+2],al
	mov	ax,cs
	mov	ds,ax
	ret

;------------------------------------------------------------------------------
;	DMA 割り込み処理
;------------------------------------------------------------------------------
dmaint:	push	ax
	push	dx
	push	ds

	cld
	mov	ax,cs
	mov	ds,ax

;	DMA割り込みかどうか判定
	mov	dx,0f46h
	in	al,dx
	test	al,1
	jz	dmaint_exit

;	DMAプログラム
	call	dma_program

;	次のデータを読んでくれフラグセット
	mov	[filenext_flag],1

;	FIFO割り込みフラグリセット
	mov	dx,0f46h
	out	dx,al

dmaint_exit:
	call	put_eoi
	pop	ds
	pop	dx
	pop	ax
	iret

;------------------------------------------------------------------------------
;	EOI send
;------------------------------------------------------------------------------
put_eoi:
	mov	al,0bh		;ISR readmode master
	out	00h,al
	out	5fh,al
	in	al,00h		;ISR read master
	or	al,al
	js	put_eoi_slave
put_eoi_master:
	mov	al,20h		;EOI send master
	out	00h,al
	ret
put_eoi_slave:
	mov	al,20h		;EOI send slave
	out	08h,al
	out	5fh,al
	mov	al,0bh		;ISR readmode slave
	out	08h,al
	out	5fh,al
	in	al,08h		;ISR read slave
	or	al,al
	jz	put_eoi_master
	ret

;------------------------------------------------------------------------------
;	DMAプログラム
;------------------------------------------------------------------------------
dma_program:
;	DMAマスクビットをセット
	mov	al,4
	or	al,[dmanum]
	out	15h,al		;MaskSet

;	DMAモードを設定
	mov	al,48h
	or	al,[dmanum]
	out	17h,al		;ModeSet (single/read trans)

	push	bx

	mov	bx,offset buffer1ofs
	cmp	[bufferflag],1
	jz	dma_set_adr
	mov	bx,offset buffer2ofs
dma_set_adr:

;	DMAバンクレジスタを設定
	mov	dx,[dma_bank]
	mov	al,[bx+2]
	out	dx,al

;	クリアバイトポインタフリップフロップを実行
	out	19h,al

;	DMAアドレスを設定
	mov	dx,[dma_adr]
	mov	al,[bx]
	out	dx,al
	mov	al,[bx+1]
	out	dx,al

	pop	bx

;	DMAカウンタを設定
	mov	dx,[dma_count]
	mov	ax,bufsize
	out	dx,al
	mov	al,ah
	out	dx,al

;	DMAマスクビットをクリア
	mov	al,[dmanum]
	out	15h,al		;MaskSet

	ret

;------------------------------------------------------------------------------
;	wssレジスタ設定 ah=reg al=data
;------------------------------------------------------------------------------
wssset:	push	dx
	push	ax
	mov	dx,0f44h
	xchg	ah,al
	or	al,020h	;TRD bit on
	out	dx,al	;set REG
	inc	dx
	xchg	ah,al
	out	dx,al	;set DATA
	pop	ax
	pop	dx
	ret

;------------------------------------------------------------------------------
;	wssの INIT フラグを待つ [時限付き]
;------------------------------------------------------------------------------
wsswait:
	push	ax
	push	cx
	push	dx
	mov	cx,10000
	mov	dx,0f44h
wsswait_loop:
	in	al,dx
	test	al,al
	jns	wsswait_end
	loop	wsswait_loop
wsswait_end:
	pop	dx
	pop	cx
	pop	ax
	ret

;------------------------------------------------------------------------------
;	バッファに16KB読み込み
;------------------------------------------------------------------------------
read_buffer:
	mov	dx,offset buffer1	;バッファ選択
	xor	[bufferflag],1
	jnz	readbuffer_main
	mov	dx,offset buffer2

readbuffer_main:
	mov	di,dx
	mov	cx,bufsize /2
	mov	ax,[nulldata]
rep	stosw				;buffer clear

;	残りサイズ計算
	cmp	word ptr [chunk_size+2],0
	jnz	readbuf_1
	cmp	word ptr [chunk_size],bufsize+1
	jc	readbuf_2
readbuf_1:
	sub	word ptr [chunk_size],bufsize
	sbb	word ptr [chunk_size+2],0

	mov	cx,bufsize
	mov	bx,[fhand]
	mov	ah,3fh
	push	dx
	int	21h			;read
	pop	dx
	jc	fileerr
	cmp	ax,bufsize		;最後を読んだか？
	jz	readbuf_exit
	mov	[fileend_flag],1	;最後を読んだフラグセット
	jmp	readbuf_exit

readbuf_2:				;最後の16KB未満
	mov	cx,word ptr [chunk_size]
	mov	bx,[fhand]
	mov	ah,3fh
	push	dx
	int	21h			;read
	pop	dx
	jc	fileerr
	mov	[fileend_flag],1	;最後を読んだフラグセット

readbuf_exit:
	cmp	[sign_flag],1
	jnz	readbuf_ret

;	.PC8の時は符号なしに変換
	mov	si,dx
	mov	di,dx
	mov	cx,bufsize
sign_loop:
	lodsb
	add	al,80h
	stosb
	loop	sign_loop

readbuf_ret:
	ret

;------------------------------------------------------------------------------
;	PCMファイル種別確認
;------------------------------------------------------------------------------
get_pcminfo:
	mov	dx,offset buffer1
	mov	cx,12 + 80h
	call	readfile
	jc	fileerr

;------------------------------------------------------------------------------
;	WAVE format check
;------------------------------------------------------------------------------
	mov	bx,offset buffer1
	call	check_wave_header
	jnc	form_wave
	add	bx,80h
	call	check_wave_header
	jc	no_wav
	add	[chunk_adr],80h

;------------------------------------------------------------------------------
;	WAVE format
;------------------------------------------------------------------------------
form_wave:
;	[fmt ] チャンク
	mov	si,"mf"
	mov	di," t"
	call	search_chunk

	mov	dx,offset buffer1
	mov	cx,word ptr [chunk_size]
	call	readfile
	jc	form_err

;	format確認
	mov	si,offset buffer1
	mov	ax,format_id[si]
	cmp	ax,1			;リニア
	jnz	no_linear
	mov	[nulldata],8080h	;null=80h
	xor	ax,ax
	cmp	samplebits[si],8	;8bit
	jz	set_format

	mov	[nulldata],0		;null=0
	mov	[bit16_flag],1
	mov	al,01000000b
	cmp	samplebits[si],16	;16bit
	jz	set_format
	jmp	form_err
no_linear:

	cmp	ax,6			;A-law
	jnz	no_alaw
	cmp	samplebits[si],8	;8bit
	jnz	form_err
	mov	[nulldata],0d5d5h
	mov	al,01100000b
	jmp	set_format
no_alaw:

	cmp	ax,7			;u-law
	jnz	no_ulaw
	cmp	samplebits[si],8	;8bit
	jnz	form_err
	mov	[nulldata],0ffffh
	mov	al,00100000b
	jmp	set_format
no_ulaw:

	cmp	ax,11h			;ima ADPCM
	jnz	form_err
	cmp	samplebits[si],4	;4bit
	jnz	form_err
	mov	[adpcm_flag],1
	mov	al,10100000b

set_format:
	mov	[pcm_form_rate],al

;	rate 設定
	mov	dx,64000
	cmp	word ptr samplerate+2[si],0
	jnz	rate_calc		;65536Hz以上
	mov	dx,word ptr samplerate[si]
rate_calc:
	call	set_rate

;	channel 設定
	cmp	channel[si],2
	jc	search_data
	jnz	form_err
	or	[pcm_form_rate],00010000b	;stereo
	mov	[stereo_flag],1

;	dataチャンク検索
search_data:
	mov	si,"ad"
	mov	di,"at"
	jmp	search_chunk

;------------------------------------------------------------------------------
;	AIFF format check
;------------------------------------------------------------------------------
no_wav:
	mov	bx,offset buffer1
	call	check_aiff_header
	jnc	form_aifc
	add	bx,80h
	call	check_aiff_header
	jc	no_aiff
	add	[chunk_adr],80h

;------------------------------------------------------------------------------
;	AIFC format?
;------------------------------------------------------------------------------
form_aifc:
	test	ax,ax
	jz	form_aiff
	mov	[aifc_flag],1

;------------------------------------------------------------------------------
;	AIFF format
;------------------------------------------------------------------------------
form_aiff:
	mov	[aiff_flag],1
	mov	si,"OC"
	mov	di,"MM"
	call	search_chunk

	mov	dx,offset buffer1
	mov	cx,word ptr [chunk_size]
	call	readfile
	jc	form_err

	mov	si,offset buffer1
	cmp	[aifc_flag],1
	jz	check_aifc

	mov	dx,_samplebits[si]
	xchg	dh,dl
	xor	ax,ax
	mov	[sign_flag],1
	cmp	dx,8			;8bit linear
	jz	fa_set_form
	mov	[sign_flag],0

	mov	[bit16_flag],1
	mov	al,11000000b
	cmp	dx,16			;16bit linear
	jz	fa_set_form
	jmp	form_err

check_aifc:
	cmp	word ptr _codec[si],"lu"	;u-law
	jnz	fa_no_ulaw
	cmp	word ptr _codec[si+2],"wa"
	jnz	form_err
	cmp	_samplebits[si],1000h		;16bit
	jnz	form_err
	mov	[nulldata],0ffffh
	mov	al,00100000b
	jmp	fa_set_form
fa_no_ulaw:

	cmp	word ptr _codec[si],"mi"	;ima4
	jnz	form_err
	cmp	word ptr _codec[si+2],"4a"
	jnz	form_err
	cmp	_samplebits[si],1000h		;16bit
	jnz	form_err
	mov	[adpcm_flag],1
	mov	al,10100000b
	jmp	fa_set_form

fa_set_form:
	mov	[pcm_form_rate],al

	mov	dx,_samplerate[si]		;freq.
	xchg	dh,dl
	cmp	_unknown[si],0eh		;<謎> 0D以下なら
	jnc	fa_sr_set
	mov	cl,0eh
	sub	cl,_unknown[si]
	shr	dx,cl				;freq./(0eh-<謎>)^2
fa_sr_set:
	call	set_rate

	mov	dx,_channel[si]			;ch.
	xchg	dh,dl
	cmp	dx,2
	jc	fa_search_data
	jnz	form_err
	or	[pcm_form_rate],00010000b	;stereo
	mov	[stereo_flag],1

fa_search_data:
	mov	si,"SS"
	mov	di,"DN"
	call	search_chunk

;	8byte skip
	xor	cx,cx
	mov	dx,8
	mov	bx,[fhand]
	mov	ax,4201h
	int	21h
	sub	word ptr [chunk_size],8
	sbb	word ptr [chunk_size+2],0

	ret

;------------------------------------------------------------------------------
;	リニアPCM
;------------------------------------------------------------------------------
no_aiff:cmp	[def_type],-1
	jz	check_ext_type
	mov	al,[def_type]
	test	al,al
	jz	form_pc8
	dec	al
	jz	form_spk
	dec	al
	jz	form_phl
	dec	al
	jz	form_p16
	dec	al
	jz	form_alw
	dec	al
	jz	form_ulw
	dec	al
	jz	form_ima
	jmp	usage

check_ext_type:
	mov	ah,ds:[bp]
	mov	bx,ds:[bp+1]
	and	ah,11011111b		;小文字＞大文字変換
	and	bx,1101111111011111b	;//

	cmp	ah,"P"
	jnz	lp_01
	cmp	bx,256*("6"-20h)+("1"-20h)
	jz	form_p16
	cmp	bx,256*("8"-20h)+"C"
	jz	form_pc8
	jmp	form_phl
lp_01:	cmp	ah,"S"
	jnz	form_pc8
	cmp	bx,"KP"
	jz	form_spk

form_pc8:			;8bit Sign (default)
	mov	[sign_flag],1
	xor	ax,ax
	jmp	linear_set_form
form_spk:			;8bit no-Sign
	mov	[nulldata],8080h	;null=80h
	xor	ax,ax
	jmp	linear_set_form
form_phl:			;16bit Big Endian
	mov	al,11000000b
	jmp	bit16
form_p16:			;16bit Little Endian
	mov	al,01000000b
bit16:	mov	[bit16_flag],1
	jmp	linear_set_form

form_alw:
	mov	[nulldata],0d5d5h
	mov	al,01100000b
	jmp	linear_set_form
form_ulw:
	mov	[nulldata],0ffffh
	mov	al,00100000b
	jmp	linear_set_form
form_ima:
	mov	[adpcm_flag],1
	mov	al,10100000b

linear_set_form:
	mov	[pcm_form_rate],al

	mov	dx,[def_rate]
	call	set_rate

	cmp	[def_ch],1
	jz	size_calc
	or	[pcm_form_rate],00010000b	;stereo
	mov	[stereo_flag],1

size_calc:
	xor	cx,cx
	mov	dx,cx
	mov	bx,[fhand]
	mov	ax,4202h
	int	21h		;file pointer to tail
	jc	fileerr

	mov	word ptr [chunk_size],ax
	mov	word ptr [chunk_size +2],dx

	xor	cx,cx
	mov	dx,cx
	mov	bx,[fhand]
	mov	ax,4200h
	int	21h		;file pointer to top

	ret

;------------------------------------------------------------------------------
;	Check .WAV Header
;		in. bx offset
;		out.cy
;------------------------------------------------------------------------------
check_wave_header:
	cmp	word ptr [bx],"IR"	
	jnz	chkwav_err
	cmp	word ptr [bx +2],"FF"
	jnz	chkwav_err
	cmp	word ptr [bx +8],"AW"
	jnz	chkwav_err
	cmp	word ptr [bx +10],"EV"
	jnz	chkwav_err
	clc
	ret
chkwav_err:
	stc
	ret

;------------------------------------------------------------------------------
;	Check .AIF Header
;		in. bx offset
;		out.cy , ax=0 AIFF ax=1 AIFC
;------------------------------------------------------------------------------
check_aiff_header:
	cmp	word ptr [bx],"OF"	
	jnz	chk_aif_err
	cmp	word ptr [bx +2],"MR"
	jnz	chk_aif_err
	cmp	word ptr [bx +8],"IA"
	jnz	chk_aif_err
	xor	ax,ax
	cmp	word ptr [bx +10],"FF"
	jz	chk_aif_ok
	inc	ax
	cmp	word ptr [bx +10],"CF"
	jnz	chk_aif_err
chk_aif_ok:
	clc
	ret
chk_aif_err:
	stc
	ret

;------------------------------------------------------------------------------
;	レート設定
;		in. dx rate
;		out.[pcm_form_rate] < or
;------------------------------------------------------------------------------
set_rate:
	push	si
	push	di
	mov	si,offset rate_table
	mov	di,offset rate_table2
set_rate_loop:
	lodsw
	cmp	dx,ax
	jnc	sr_set
	inc	di
	jmp	set_rate_loop
sr_set:
	mov	al,[di]
	or	[pcm_form_rate],al
	pop	di
	pop	si
	ret

;------------------------------------------------------------------------------
;	file読み込み in.dx:offset cx:bytes
;------------------------------------------------------------------------------
readfile:
	mov	bx,[fhand]
	mov	ah,3fh
	int	21h			;read
	ret

;------------------------------------------------------------------------------
;	チャンク検索
;	in.	si:di = chunk name
;	out.	file pointer to chunk start
;		[chunk_size] to chunk size
;------------------------------------------------------------------------------
search_chunk:
	xor	cx,cx
	mov	dx,[chunk_adr]		;chunk.start
	mov	bx,[fhand]
	mov	ax,4200h
	int	21h			;move.pointer

search_chunk_loop:
	mov	dx,offset buffer1
	mov	cx,8
	call	readfile
	jc	form_err
	cmp	si,word ptr [buffer1]
	jnz	skip_chunk
	cmp	di,word ptr [buffer1+2]
	jnz	skip_chunk

;---------------------------------------
;(local-sub) get chunk size
;---------------------------------------
chunk_size_calc:
	mov	si,offset buffer1+4
	mov	di,offset chunk_size
	cmp	[aiff_flag],1
	jz	size_aiff
	movsw
	movsw
	ret
size_aiff:
	mov	ax,2[si]
	xchg	ah,al
	stosw
	lodsw
	xchg	ah,al
	stosw
	ret

skip_chunk:
	push	si
	push	di
	call	chunk_size_calc
	pop	di
	pop	si
	mov	dx,word ptr [chunk_size]
	mov	cx,word ptr [chunk_size+2]
	mov	bx,[fhand]
	mov	ax,4201h
	int	21h			;move.pointer
	jc	form_err
	jmp	search_chunk_loop

;==============================================================================
;	ファイル名の取得	区切り記号 = comma/space/esccode(00,tab,cr)
;			in.	ds:si	取得元
;				es:di	取得先
;				es:bx	拡張子無しの場合に付ける拡張子のofs
;			out.	bp	拡張子先頭のoffset address
;==============================================================================
get_filename:
	lodsb
	cmp	al," "
	jz	get_filename
	cmp	al,9
	jz	get_filename
	dec	si

	mov	bp,-1		;"."があったか？flag
gf_loop:
	lodsb
	call	sjis_check
	jnc	gf_notsjis
	stosb		;S-JIS漢字1byte目なら 無条件に書き込み
	movsb		;S-JIS漢字2byte目を転送
	jmp	gf_loop
gf_notsjis:
	cmp	al,"!"
	jc	gf_plus
	cmp	al,","
	jz	gf_plus
	stosb
	cmp	al,"\"
	jz	gf_yen
	cmp	al,"."
	jz	gf_period
	jmp	gf_loop
gf_yen:
	mov	bp,-1		;"\"があったら "."flagをreset
	jmp	gf_loop
gf_period:
	mov	bp,di		;"."があった
	jmp	gf_loop
gf_plus:
	cmp	bp,-1		;"."があったか？
	jnz	gf_exit
	mov	al,"."
	stosb
	mov	bp,di
	mov	ax,es:[bx]
	stosw
	mov	al,es:2[bx]
	stosb
gf_exit:
	xor	al,al
	stosb
	ret

;==============================================================================
;	al が shiftJIS 1byte目かどうかcheck
;			output	cy=1	ShiftJIS
;==============================================================================
sjis_check:
	or	al,al
	jns	sjc_clc_ret
	push	ax
	sub	al,20h
	test	al,40h
	pop	ax
	jz	sjc_clc_ret
	stc
	ret
sjc_clc_ret:
	clc
	ret

;==============================================================================
;	[si]から数値を読み出す 16進対応
;	数字が書かれていない場合はcy=1になる
;		output	al/bx
;==============================================================================
get_num:
	xor	bx,bx
	cmp	byte ptr [si],"$"
	jz	hg01

;	10進の場合
	call	numget
	mov	bl,al
	jnc	lng1
	stc		;1桁めが書いてない
	ret

lng1:	call	numget
	jnc	lng2

	clc
lnexit:	mov	al,bl
	ret

lng2:	push	dx
	add	bx,bx
	mov	dx,bx
	add	bx,bx
	add	bx,bx
	add	bx,dx	;bx=bx*10
	pop	dx

	add	bl,al
	adc	bh,0	;bx=bx+al

	jmp	lng1

;	16進の場合
hg01:	inc	si
	call	hexget
	mov	bl,al
	jnc	hg02
	stc		;1桁めが書いてない
	ret

hg02:
	call	hexget
	jnc	hg00
	mov	al,bl
	clc
	ret

hg00:	add	bx,bx
	add	bx,bx
	add	bx,bx
	add	bx,bx	;bx=bx*16
	add	bl,al

	jmp	hg02

;==============================================================================
;	0 ～ F の数値を得る
;		inputs	-- ds:si to pointer
;		outputs	-- al
;			-- cy [1=error]
;==============================================================================
hexget:
	lodsb
	cmp	al,"a"
	jc	hexget_01
	sub	al,"a"-"A"	;小文字は大文字に変換

hexget_01:
	sub	al,"0"
	jc	hexget_err
	cmp	al,10
	jc	hexget_02
	sub	al,"A"-"9"-1
	jc	hexget_err
	cmp	al,10
	jc	hexget_err
hexget_02:
	cmp	al,16
	jnc	hexget_err
	clc
	ret
hexget_err:
	dec	si
	stc
	ret

;==============================================================================
;	0 ～ 9 の数値を得る
;		inputs	-- ds:si to pointer
;		outputs	-- al
;			-- cy [1=error]
;==============================================================================
numget:	lodsb
	sub	al,"0"
	jc	nmerr
	cmp	al,10
	jnc	nmerr
	clc
	ret
nmerr:	dec	si
	stc
	ret

;------------------------------------------------------------------------------
;	Dataarea
;------------------------------------------------------------------------------
title_mes	db	"-- WSS PCM Player -- ver.",ver,9
		db	"Programmed by M.Kajihara(KAJA) ",date,13,10
		db	13,10,"$"
fin_mes		db	"Complete.",13,10,"$"
nowss_mes	db	"WSS does not exist",13,10,"$"
wsserr_mes	db	"WSS error",13,10,"$"
formerr_mes	db	"PCM format error",13,10,"$"
fileerr_mes	db	"File Read Error",13,10,"$"
usage_mes	db	"usage : wp [{/|-}option] filename[.wav]",13,10,13,10
		db	"option: /v[num] Volume def:63",13,10
		db	9,"/d[num] DMA ch# (0,1,3)",13,10
		db	9,"/i[num] INT # (0,1,41,5)",13,10
		db	9,"/s",9,"Stereo def:Mono",13,10
		db	9,"/f[num] Rate[Hz] def:44100",13,10
		db	9,"/t[num] Type 0=pc8 1=spk 2=phl 3=p16 4=u-law 5=a-law 6=ima4adpcm def:0",13,10
		db	"$"

rate_table	dw	64000
		dw	54840
		dw	48000
		dw	44100
		dw	37800
		dw	33075
		dw	32000
		dw	27420
		dw	22050
		dw	18900
		dw	16000
		dw	11025
		dw	9600
		dw	8000
		dw	6620
		dw	0	;5510

rate_table2	db	1010b
		db	1000b
		db	1100b
		db	1011b
		db	1001b
		db	1101b
		db	0110b
		db	0100b
		db	0111b
		db	0101b
		db	0010b
		db	0011b
		db	1110b
		db	0000b
		db	1111b
		db	0001b

int_table	db	0bh,02h,00001000b,0	;INT0
		db	0dh,02h,00100000b,0	;INT1
		db	12h,0ah,00000100b,0	;INT41
		db	14h,0ah,00010000b,0	;INT5
dma_table	db	00h,27h,01h,03h		;DMA#0
		db	01h,21h,05h,07h		;DMA#1
		db	03h,25h,0dh,0fh		;DMA#3

wav_ext		db	"WAV"

def_rate	dw	44100
def_ch		db	1
def_type	db	-1	;pc8,spk,phl,p16,ulw,alw,ima
def_dma		db	-1	;0,1,3
def_int		db	-1	;0,1,4,5

chunk_adr	dw	12	;default 1st chunk offset

initarea	label	word
bufferflag	db	?
fileend_flag	db	?
filenext_flag	db	?
aiff_flag	db	?
adpcm_flag	db	?
bit16_flag	db	?
stereo_flag	db	?
sign_flag	db	?
aifc_flag	db	?
volume		db	?
nulldata	dw	?

int_vec		db	?
int_port	dw	?
int_mask	db	?

dmanum		db	?
dma_bank	dw	?
dma_adr		dw	?
dma_count	dw	?

intvec_seg	dw	?
intvec_ofs	dw	?

chunk_size	dd	?

pcm_form_rate	db	?
pcm_volume	db	?

fhand		dw	?
buffer1ofs	dw	?
buffer1ofs2	db	?
buffer2ofs	dw	?
buffer2ofs2	db	?
initend		label	word

filename	db	256 dup (?)
;align	16
buffer1		db	bufsize dup(?)
buffer2		db	bufsize dup(?)

wave_fmt	struc
format_id	dw	?
channel		dw	?
samplerate	dd	?
dataspeed	dd	?
blocksize	dw	?
samplebits	dw	?
wave_fmt	ends

aiff_fmt	struc
_channel	dw	?
_samplesize	dd	?
_samplebits	dw	?
		db	?
_unknown	db	?
_samplerate	dw	?
		db	6 dup(?)
_codec		dd	?
aiff_fmt	ends

code	ends
end	start

