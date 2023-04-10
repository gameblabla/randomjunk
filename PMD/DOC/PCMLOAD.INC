;==============================================================================
;
;	PPS/PCM�t�@�C���̓ǂݍ��� PCM��������WORK�Ɏg��Version / Include�p
;	PMDPCM.EXE/PMP.COM/MC.EXE �p
;	x8 bit�p�ɕύX /ver.4.4�ȍ~
;	.P86������ǉ� /ver.4.5�ȍ~
;	PPZ8�p������ǉ� / ver.4.8�ȍ~
;	PPZ8ADPCM�p������ǉ� / ver.4.8k�ȍ~
;
;	DISKPMD.INC��\��include�����Ă������ƁB
;	cs:[kankyo_seg]�Ɋ��̃Z�O�����g�����Ă������ƁB
;
;==============================================================================

message		equ	1		;�G���[���b�Z�[�W��\�����邩�ۂ�
ppsdrv		equ	64h		;PPSDRV��Vector
p86drv		equ	65h		;P86DRV��Vector
ppzvec		equ	7fh		;PPZ8��Vector

;==============================================================================
;
;	PPZ(PVI/PZI)�t�@�C���̓ǂݍ���
;
;		input	DS:AX	filename	(128byte)
;			CL	�ǂ܂���o���N	(1=�P�� 2=�Q�� 3=����)
;		output	cy=1	Not Loaded
;				AX=1	�t�@�C���̓ǂݍ��ݎ��s
;				AX=2	�f�[�^�`�����Ⴄ
;				AX=3	���������m�ۂł��Ȃ�
;				AX=4	EMS�n���h���̃}�b�s���O���ł��Ȃ�
;				AX=5	PPZ8���풓���Ă��Ȃ�
;				CL	�G���[�̏o��PCM�ԍ�(0 or 1)
;		break	ax,cx
;==============================================================================
ppz_load:
	push	bx
	push	dx
	push	si
	push	di
	push	bp
	push	ds
	push	es

	mov	cs:[ppz_bank],cl
	mov	cs:[filename_ofs],ax
	mov	ax,ds
	mov	cs:[filename_seg],ax

	call	ppz_load_main

	pop	es
	pop	ds
	pop	bp
	pop	di
	pop	si
	pop	dx
	pop	bx
	ret

;==============================================================================
;	PPZ8 �ǂݍ��� main
;==============================================================================
ppz_load_main:
	call	ppz8_check
	mov	cl,0
	mov	ax,4
	jc	ppz_load_error

;	PCM�Q�ǂݗp�ǉ����ʏ���
	call	read_ppz8
	jc	plm_exit
	mov	ds,cs:[filename_seg]
	lodsb
	cmp	al,","
	jnz	plm_exit2	;PCM�͂P����
	mov	cs:[filename_ofs],si
	mov	cl,1
	call	read_ppz8
plm_exit:
	ret
plm_exit2:
	xor	ax,ax
	ret

read_ppz8:
;	�g���q���� (PVI / PZI)
	lds	si,dword ptr cs:[filename_ofs]
	xor	dx,dx
p8_extcheck_loop:
	lodsb
	call	sjis_check	;in DISKPMD.INC
	jnc	p8_extcheck_nonsjis
	inc	si		;2byte�ڂ��΂�
	jmp	p8_extcheck_loop
p8_extcheck_nonsjis:
	or	al,al
	jz	p8_extcheck_exit
	cmp	al,","
	jz	p8_extcheck_exit
	cmp	al,"."
	jnz	p8_extcheck_loop
	mov	dx,si
	jmp	p8_extcheck_loop
p8_extcheck_exit:

	dec	si		;si= 00h or "," �̈ʒu
	shr	cs:[ppz_bank],1
	jnc	p8_load_skip	;load skip

	mov	ch,1		;PZI load
	test	dx,dx
	jz	p8_load_main	;�g���q�ȗ��� = PZI

	mov	bx,dx
	mov	al,1[bx]
	and	al,11011111b	;���������啶���ϊ�
	cmp	al,"V"
	jnz	p8_load_main
	dec	ch		;PVI load (ch=0)

;	PVI / PZI �ǂݍ���
p8_load_main:
	push	ds:[si]
	mov	byte ptr [si],0		; "," �Ȃ� 00h �Ɉꎞ�I�ɏ�������
	mov	ax,0300h
	mov	dx,cs:[filename_ofs]
	int	ppzvec			; pcm load
	jnc	p8_load_exit
	cmp	ax,2
	cmc
	jc	p8_load_exit		; file not found or �`�����Ⴄ�Ȃ�
	xor	ch,1			; �����Е��̌`����
	mov	ax,0300h
	int	ppzvec			; pcm load�������Ă݂�
p8_load_exit:
	pop	ds:[si]
	jc	ppz_load_error
p8_load_skip:
	xor	ax,ax
	ret

;	Error����
ppz_load_error:
	inc	ax
if	message
	mov	bx,ax
	mov	dx,offset exit1z_mes
	dec	bx
	jz	ppz_error_main

	mov	dx,offset exit2z_mes
	dec	bx
	jz	ppz_error_main

	mov	dx,offset exit3z_mes
	dec	bx
	jz	ppz_error_main

	mov	dx,offset exit4z_mes
	dec	bx
	jz	ppz_error_main

	mov	dx,offset exit5z_mes
	jmp	ppz_error_main2

ppz_error_main:
	mov	al,cl
	add	al,"1"
	mov	cs:[banknum],al	;Bank
	push	dx
	mov	dx,offset ppzbank_mes
	call	ppz_error_main2
	pop	dx
ppz_error_main2:
	push	ds
	push	ax
	mov	ax,cs
	mov	ds,ax
	mov	ah,9
	int	21h
	pop	ax
	pop	ds
endif
	stc
	ret

;	PPZ8�풓check
ppz8_check:
	xor	bx,bx
	mov	es,bx
	les	bx,es:[ppzvec*4]
	cmp	word ptr es:2[bx],"PP"
	jnz	no_ppz
	cmp	word ptr es:4[bx],"8Z"
	jnz	no_ppz
	cmp	byte ptr es:6[bx],0
	jnz	no_ppz
	clc
	ret
no_ppz:	stc
	ret

;==============================================================================
;
;	PCM(PPC/P86)�t�@�C���̓ǂݍ���
;		P86DRV.COM���풓���Ă���� .P86���A
;		�����łȂ��ꍇ�� .PPC��ǂށB
;		PMDPPZE���풓���Ă���ꍇ�͖�������PVI��PPZ8�ɓǂݍ��ށB
;
;		input	DS:AX	filename	(128byte)
;			ES:DI	pcm_work	(32KB,P86�̏ꍇ�͕K�v����)
;		output	cy=1	Not Loaded
;			PMDB2/PMD86�̏ꍇ
;				AX=1	SPB/ADPCM-RAM��PMDB2���Ȃ�
;					86B/P86DRV   ��PMD86���Ȃ�
;				AX=2	�t�@�C�����Ȃ�
;				AX=3	�t�@�C����PMD��PCM�f�[�^�ł͂Ȃ�
;				AX=4	SPB/���ɓǂݍ���ł���̂Ɠ���������
;					86B/�e��OVER
;				AX=5	�t�@�C�����ǂ߂Ȃ�
;				AX=6	PCM���������A�N�Z�X��������
;			PMDPPZE�̏ꍇ
;				AX=1	�t�@�C���̓ǂݍ��ݎ��s
;				AX=2	�f�[�^�`�����Ⴄ
;				AX=3	���������m�ۂł��Ȃ�
;				AX=4	EMS�n���h���̃}�b�s���O���ł��Ȃ�
;				AX=5	PPZ8���풓���Ă��Ȃ�
;
;	.PPC format:
;		WORK=PMD��PCMWORK , DATA=PCMRAM�擪��WORK , FILE=PCMFILE
;					 123456789012345678901234567890
;		DATA/FILE�̂�		"ADPCM DATA for  PMD ver.4.4-  "30bytes
;		WORK/DATA/FILE		1Word		Next START Address
;					2Word*256	START/STOP
;		WORK/DATA�̂�		128bytes	FILENAME
;		DATA�̂�		32bytes		�\��
;
;		PCMRAM_Work		=00000H�`00025H
;		PCMRAM_Main_data	=00026H�`01FFFH
;
;	.P86 format:
;		"PCM86 DATA",0ah,0	12 byte
;		P86DRV��version		1  byte
;		�S�̂̃T�C�Y		3  byte
;		���Ftable		start(3),size(3) * 256 (1536) bytes
;		���F�f�[�^		��
;
;==============================================================================
pcm_all_load	proc

	assume	ds:nothing,es:nothing

	cld
	push	ds
	push	es
	push	bx
	push	cx
	push	dx
	push	si
	push	di
	push	bp

	mov	cs:[filename_ofs],ax
	mov	ax,ds
	mov	cs:[filename_seg],ax
	mov	cs:[pcmdata_ofs],di
	mov	ax,es
	mov	cs:[pcmdata_seg],ax
	mov	ah,0eh		;GET_PCM_ADR
	int	60h		;DS:DX=PCM���[�N
	mov	cs:[pcmwork_ofs],dx
	mov	ax,ds
	mov	cs:[pcmwork_seg],ax

	call	all_load

	pop	bp
	pop	di
	pop	si
	pop	dx
	pop	cx
	pop	bx
	pop	es
	pop	ds
	ret

pcm_all_load	endp

;==============================================================================
;
;	PPS�t�@�C���̓ǂݍ���
;
;		input	DS:AX	filename	(128byte)
;		output	cy=1	AX=1	PPSDRV���풓���Ă��Ȃ�
;				AX=2	�t�@�C��������
;				AX=3	PPSDRV�̗e�ʂ��I�[�o�[����
;				AX=5	�t�@�C�����ǂ߂Ȃ�
;
;	PPSDATA�\��
;			2Word*14	Start address/Length
;			�ȍ~		PPS Data
;
;==============================================================================
pps_load	proc
	assume	ds:nothing,es:nothing
	cld
	push	ds
	push	es
	push	bx
	push	cx
	push	dx
	push	si
	push	di
	push	bp

	mov	cs:[filename_ofs],ax
	mov	ax,ds
	mov	cs:[filename_seg],ax

	call	ppsdrv_check
	jc	not_load

pps_load_exec:
	mov	ah,4
	int	ppsdrv
	mov	cs:[pcmdata_ofs],dx
	mov	ax,ds
	mov	cs:[pcmdata_seg],ax
	mov	cs:[pcmdata_size],cx

	call	pps_load_main

not_load:
	pop	bp
	pop	di
	pop	si
	pop	dx
	pop	cx
	pop	bx
	pop	es
	pop	ds
	ret

pps_load	endp

;==============================================================================
;	pps load
;		in	cs:[filename_ofs/seg]	Filename
;			cs:[pcmdata_ofs/seg]	PPSData�ʒu
;			cs:[pcmdata_size]	PPSData�e��
;==============================================================================
pps_load_main	proc
	call	filename_set

;-----------------------------------------------------------------------------
;	File Read
;-----------------------------------------------------------------------------
	mov	bx,offset pps_ext
	call	ext_set		;�g���q "PPS"�ɕύX
	mov	ax,cs
	mov	ds,ax
	mov	dx,offset filename_buf
	mov	es,[kankyo_seg]
	call	opnhnd		;Open
	jnc	exec_ppsload

	lds	dx,dword ptr cs:[filename_ofs]
	mov	es,cs:[kankyo_seg]
	call	opnhnd		;Open
	jc	allload_exit2p	;File������

exec_ppsload:
	lds	dx,dword ptr cs:[pcmdata_ofs]
	mov	cx,cs:[pcmdata_size]
	inc	cx		;1byte�]�v�ɓǂ�
	push	cx
	call	redhnd
	pop	cx
	jc	allload_exit5p_close	;file���ǂ߂Ȃ�
	cmp	ax,cx
	jz	allload_exit3p_close	;�e�ʃI�[�o�[

	call	clohnd
	xor	ax,ax
	ret

pps_load_main	endp

;==============================================================================
;	.PPC/.P86 �ꊇload
;		in	cs:[filename_ofs/seg]	Filename
;			cs:[pcmdata_ofs/seg]	PCMData loadarea
;			cs:[pcmwork_ofs/seg]	PMD��PCMwork
;==============================================================================
all_load	proc
;-----------------------------------------------------------------------------
;	�ǂݍ��ނ̂�.P86��.PPC���ǂ����𔻕�
;-----------------------------------------------------------------------------
	call	check_p86drv
	jnc	p86_load
	mov	ah,09h
	int	60h		;board check
	cmp	al,1		;pmdb2
	jz	allload_main1
	cmp	al,4		;pmdppz
	jz	allload_main1
	cmp	al,5		;pmdppze
	jz	allload_ppze
	jmp	allload_exit1

;-----------------------------------------------------------------------------
;	.PPC read Main
;-----------------------------------------------------------------------------
allload_main1:
	call	check_pmdb2
	jc	allload_exit1
	call	filename_set

;-----------------------------------------------------------------------------
;	File��PMD�̃��[�N�Ƀw�b�_�����ǂ݂���
;-----------------------------------------------------------------------------
	mov	bx,offset ppc_ext
	call	ext_set		;�g���q "PPC"�ɕύX
	mov	ax,cs
	mov	ds,ax
	mov	dx,offset filename_buf
	mov	es,[kankyo_seg]
	call	opnhnd		;Open
	jnc	exec_ppcload

	lds	dx,dword ptr cs:[filename_ofs]
	mov	es,cs:[kankyo_seg]
	call	opnhnd		;Open
	jnc	exec_ppcload

	mov	bx,offset p86_ext
	call	ext_set		;�g���q "P86"�ɕύX
	mov	ax,cs
	mov	ds,ax
	mov	dx,offset filename_buf
	mov	es,[kankyo_seg]
	call	opnhnd		;Open
	jc	allload_exit2
exec_ppcload:
	lds	dx,dword ptr cs:[pcmdata_ofs]
	mov	cx,30		;���ʕ�����
	call	redhnd
	jc	allload_exit5_close
	cmp	ax,30		;Header�̒���=�ǂݍ��񂾒������H
	jnz	allload_exit3_close

	lds	bx,dword ptr cs:[pcmdata_ofs]
	cmp	word ptr 0[bx],"VP"	;PV
	jnz	not_pvi
	cmp	word ptr 2[bx],"2I"	;I2
	jnz	not_pvi
	cmp	byte ptr 10[bx],02h	;RAM Type 8bit
	jz	pvi_load
not_pvi:
	cmp	word ptr 0[bx],"DA"	;AD
	jnz	allload_exit3_close		;PMD��PCM�f�[�^�ł͂Ȃ�
	cmp	word ptr 2[bx],"CP"	;PC
	jnz	allload_exit3_close		;PMD��PCM�f�[�^�ł͂Ȃ�
	cmp	word ptr 4[bx]," M"	;M 
	jnz	allload_exit3_close		;PMD��PCM�f�[�^�ł͂Ȃ�

	lds	dx,dword ptr cs:[pcmwork_ofs]
	mov	cx,4*256+2	;Header�̒��� (filename�͂Ȃ�)
	call	redhnd		;Read
	jc	allload_exit5_close
	cmp	ax,4*256+2	;Header�̒���=�ǂݍ��񂾒������H
	jnz	allload_exit3_close

;-----------------------------------------------------------------------------
;	PMD�̃��[�N��Filename������
;-----------------------------------------------------------------------------
ppc_load_main:
	call	write_filename_to_pmdwork

;-----------------------------------------------------------------------------
;	PCMRAM�̃w�b�_��ǂ�
;-----------------------------------------------------------------------------
	cmp	cs:[retry_flag],0
	jnz	write_pcm_main		;������
	mov	cs:[pcmstart],0
	mov	cs:[pcmstop],25h	;read 25h*32 = 1184(32+256*4+128) bytes
	call	pcmread
	jc	allload_exit6_close

;-----------------------------------------------------------------------------
;	PMD�̃��[�N��PCMRAM�̃w�b�_���r
;-----------------------------------------------------------------------------
	lds	si,dword ptr cs:[pcmdata_ofs]
	les	di,dword ptr cs:[pcmwork_ofs]
	add	si,30		;"ADPCM�`"�w�b�_���΂�
	mov	cx,+(4*256+128+2)/2
rep	cmpsw
	jz	allload_exit4_close	;��v����

;-----------------------------------------------------------------------------
;	PMD�̃��[�N��PCMRAM���ɏ�������
;-----------------------------------------------------------------------------
write_pcm_main:
	mov	ax,cs
	mov	ds,ax
	mov	si,offset adpcm_header
	les	di,dword ptr cs:[pcmdata_ofs]
	mov	cx,30/2		;"ADPCM�`"�w�b�_����������
rep	movsw
	lds	si,dword ptr cs:[pcmwork_ofs]
	mov	cx,+(4*256+128+2)/2
rep	movsw
	mov	cs:[pcmstart],0
	mov	cs:[pcmstop],25h
	call	pcmstore

;-----------------------------------------------------------------------------
;	PCMDATA��PCMRAM�ɏ�������
;	8000h���ǂݍ��݂Ȃ����`
;-----------------------------------------------------------------------------
if	message
	mov	ax,cs
	mov	ds,ax
	mov	dx,offset allload_mes
	mov	ah,09h
	int	21h		;"�o�b�l��`��"�̕\��
endif
	lds	bx,dword ptr cs:[pcmwork_ofs]
	mov	ax,ds:[bx]	;AX=PCM Next Start Address
	sub	ax,26h		;���ۂɂ��ꂩ��]������f�[�^�ʂɕϊ�

	mov	cs:[pcmstart],26h
	mov	cs:[pcmstop],426h	;400h*32=8000h �ꊇ
allload_loop:
	cmp	ax,401h
	jc	allload_last
	sub	ax,400h

	mov	bp,ax	;Push
	lds	dx,dword ptr cs:[pcmdata_ofs]
	mov	cx,8000h
	call	redhnd		;DISK Read
	jc	allload_exit5_close
	cmp	ax,8000h
	jnz	allload_exit3_close
	call	pcmstore	;PCM Store
	add	cs:[pcmstart],400h
	add	cs:[pcmstop],400h
	mov	ax,bp	;Pop
	jmp	allload_loop

allload_last:
	or	ax,ax
	jz	allload_justend
	mov	bp,ax	;Push
	add	ax,cs:[pcmstart]
	mov	cs:[pcmstop],ax
	lds	dx,dword ptr cs:[pcmdata_ofs]
	mov	cx,8000h
	call	redhnd		;DISK Read
	jc	allload_exit5_close
	mov	bx,bp	;Pop
	add	bx,bx
	add	bx,bx
	add	bx,bx
	add	bx,bx
	add	bx,bx
	cmp	ax,bx
	pushf
	call	pcmstore	;PCM Store
	popf
	jc	allload_exit3_close
allload_justend:
	call	clohnd		;FILE Close

;-----------------------------------------------------------------------------
;	�I��
;-----------------------------------------------------------------------------
	xor	ax,ax
	ret

all_load	endp

;==============================================================================
;	.PVI loading
;==============================================================================
pvi_load:
;-----------------------------------------------------------------------------
;	�w�b�_/���Ftable�̎c���ǂݍ���
;-----------------------------------------------------------------------------
	lds	dx,dword ptr cs:[pcmdata_ofs]
	add	dx,30
	mov	cx,128*4+16-30	;Header �c��
	call	redhnd
	jc	allload_exit5_close
	cmp	ax,128*4+16-30
	jnz	allload_exit3_close

;-----------------------------------------------------------------------------
;	pvi�̉��F��񂩂�pmd�֓]��
;-----------------------------------------------------------------------------
	lds	si,dword ptr cs:[pcmdata_ofs]
	add	si,16
	les	di,dword ptr cs:[pcmwork_ofs]
	add	di,2
	xor	bx,bx
	mov	cx,128
	mov	dx,26h
tableset_loop:
	lodsw
	add	ax,dx
	stosw
	lodsw
	or	ax,ax
	jnz	not_null_pcm
	sub	word ptr es:-2[di],dx
	sub	ax,dx
not_null_pcm:
	add	ax,dx
	stosw
	cmp	bx,ax
	jnc	not_set_max
	mov	bx,ax
	inc	bx
not_set_max:
	loop	tableset_loop
	xor	ax,ax
	mov	cx,128*2
rep	stosw		;�c��128�͖���`
	mov	di,cs:[pcmwork_ofs]
	mov	es:[di],bx

	jmp	ppc_load_main	;���Ƃ�ppc�Ɠ���

;==============================================================================
;	PMDPPZE�� ADPCM�p load
;==============================================================================
allload_ppze:
	call	ppz8_check
	mov	ax,4
	mov	cl,0
	jc	ppz_load_error	;PPZ8��풓

	call	filename_set
	mov	bx,offset pvi_ext
	call	ext_set		;�g���q "PVI"�ɕύX
	mov	ax,cs
	mov	ds,ax
	mov	dx,offset filename_buf
	mov	cx,0001h	;pvi,buffer 1
	mov	ax,0300h
	int	ppzvec
	jnc	ppze_load_exit
	cmp	ax,2
	jnc	ppz_load_error

	mov	bx,offset pzi_ext
	call	ext_set		;�g���q "PZI"�ɕύX
	mov	ax,cs
	mov	ds,ax
	mov	dx,offset filename_buf
	mov	cx,0101h	;pzi,buffer 1
	mov	ax,0300h
	int	ppzvec
	jc	ppz_load_error
ppze_load_exit:
	xor	ax,ax
	ret			;����I��

;==============================================================================
;	P86 data �ꊇload
;		in	cs:[filename_ofs/seg]	Filename
;==============================================================================
p86_load	proc
;-----------------------------------------------------------------------------
;	P86drv��check
;-----------------------------------------------------------------------------
	mov	ah,-2
	int	p86drv		;INT65H �g�p��?
	or	al,al
	jnz	allload_exit6
	mov	ah,-1
	int	p86drv
	cmp	al,10h		;ver.1.0�ȑO?
	jc	allload_exit6

	call	filename_set

;-----------------------------------------------------------------------------
;	P86Data,Size�m�F
;-----------------------------------------------------------------------------
	mov	bx,offset p86_ext
	call	ext_set		;�g���q "P86"�ɕύX
	mov	ax,cs
	mov	ds,ax
	mov	dx,offset filename_buf
	mov	es,[kankyo_seg]
	mov	ah,6
	int	p86drv		;Read
	test	al,al
	jz	p86load_complete

	lds	dx,dword ptr cs:[filename_ofs]
	mov	es,cs:[kankyo_seg]
	mov	ah,6
	int	p86drv		;Read
	test	al,al
	jz	p86load_complete

	mov	bx,offset ppc_ext
	call	ext_set		;�g���q "PPC"�ɕύX
	mov	ax,cs
	mov	ds,ax
	mov	dx,offset filename_buf
	mov	es,[kankyo_seg]
	mov	ah,6
	int	p86drv		;Read
	test	al,al
	jnz	p86load_error
p86load_complete:
	xor	ax,ax		;����I��
	ret

p86load_error:
	dec	al
	jz	allload_exit1	;No Resident PMD86
	dec	al
	jz	allload_exit2	;File not found
	dec	al
	jz	allload_exit4pp	;Size Over
	dec	al
	jz	allload_exit3	;Type Missmatch
	dec	al
	jz	allload_exit5	;Read Error
	jmp	allload_exit1	;Other Errors

p86_load	endp

;-----------------------------------------------------------------------------
;	�G���[���^�[��
;-----------------------------------------------------------------------------
allload_exit1:
if	message
	mov	dx,offset exit1_mes
endif
	mov	ax,1	;PCM����`�o���܂���B
	jmp	error_exec

allload_exit1p:
if	message
	mov	dx,offset exit1p_mes
endif
	mov	ax,1	;PPSDRV���풓���ĂȂ�
	jmp	error_exec

allload_exit2:
if	message
	mov	dx,offset exit2_mes
endif
	mov	ax,2	;PCM�t�@�C�����Ȃ�
	jmp	error_exec

allload_exit2p:
if	message
	mov	dx,offset exit2p_mes
endif
	mov	ax,2	;PPS�t�@�C�����Ȃ�
	jmp	error_exec

allload_exit3_close:
	call	clohnd
allload_exit3:
if	message
	mov	dx,offset exit3_mes
endif
	mov	ax,3	;�t�@�C����PMD��PCM�ł͂Ȃ�
	jmp	error_exec

allload_exit3p_close:
	call	clohnd
if	message
	mov	dx,offset exit3p_mes
endif
	mov	ax,3	;PPS�f�[�^�e�ʃI�[�o�[
	jmp	error_exec

allload_exit4_close:
	call	clohnd
if	message
	mov	dx,offset exit4_mes
endif
	mov	ax,4	;�f�[�^����v����
	jmp	error_exec

allload_exit4pp:
if	message
	mov	dx,offset exit4pp_mes
endif
	mov	ax,4	;P86�e�ʂ�����Ȃ�
	jmp	error_exec

allload_exit5_close:
	call	clohnd
allload_exit5:
if	message
	mov	dx,offset exit5_mes
endif
	mov	ax,5	;PCM�t�@�C�����ǂ߂Ȃ�
	jmp	error_exec

allload_exit5p_close:
	call	clohnd
if	message
	mov	dx,offset exit5p_mes
endif
	mov	ax,5	;PPS�t�@�C�����ǂ߂Ȃ�
	jmp	error_exec

allload_exit6_close:
	call	clohnd
allload_exit6:
if	message
	mov	dx,offset exit6_mes
endif
	mov	ax,6	;PCM�������A�N�Z�X��
	jmp	error_exec

error_exec:
if	message
	push	ax
	mov	ax,cs
	mov	ds,ax
	mov	ah,09h
	int	21h
	pop	ax
endif
	stc
	ret

;==============================================================================
;	PMDB2��ADPCM��Check
;		output	cy	PMDB2����ADPCM���Ȃ�
;==============================================================================
check_pmdb2	proc
;-----------------------------------------------------------------------------
;	PMDB2��ADPCM�̓���CHECK
;-----------------------------------------------------------------------------
	mov	ah,10h
	int	60h		;get_workadr in DS:DX
	mov	bx,dx
	mov	bx,-2[bx]	;ds:bx = open_work
	cmp	_pcm_gs_flag[bx],0
	jnz	cpb_stc_ret	;ERROR Return
	mov	ax,_fm2_port1[bx]
	mov	cs:[port46],ax
	mov	ax,_fm2_port2[bx]
	mov	cs:[port47],ax
	clc
	ret
cpb_stc_ret:
	stc
	ret
check_pmdb2	endp

;==============================================================================
;	P86DRV�̏풓Check
;		output	cy	P86DRV���Ȃ�
;==============================================================================
check_p86drv	proc
	push	es
	push	ax
	xor	ax,ax
	mov	es,ax
	les	bx,es:[p86drv*4]
	cmp	word ptr es:2[bx],"8P"
	jnz	p86drv_not_found
	cmp	byte ptr es:4[bx],"6"
	jnz	p86drv_not_found
	pop	ax
	pop	es
	clc
	ret
p86drv_not_found:
	pop	ax
	pop	es
	stc
	ret

check_p86drv	endp

;==============================================================================
;	PPSDRV �풓check
;			output	cy=1	�풓���ĂȂ�
;==============================================================================
ppsdrv_check	proc
	xor	ax,ax
	mov	es,ax
	les	bx,es:[ppsdrv*4]
	cmp	word ptr es:2[bx],"MP"
	jnz	allload_exit1p
	cmp	byte ptr es:4[bx],"P"
	jnz	allload_exit1p
	clc
	ret
ppsdrv_check	endp

;==============================================================================
;	Filename�̑啶�������p�X���������
;==============================================================================
filename_set	proc
;-----------------------------------------------------------------------------
;	Filename������������啶���ɕϊ�(SHIFTJIS���t��)
;-----------------------------------------------------------------------------
	lds	si,dword ptr cs:[filename_ofs]
exbig_loop:
	lodsb
	call	sjis_check	;in DISKPMD.INC
	jnc	exbig_nonsjis
	inc	si		;2byte�ڂ��΂�
	jmp	exbig_loop
exbig_nonsjis:
	or	al,al
	jz	exbig_exit
	cmp	al,"a"
	jc	exbig_loop
	cmp	al,"z"+1
	jnc	exbig_loop
	and	al,11011111b	;���������啶���ϊ�
	mov	ds:-1[si],al	;���ď�����
	jmp	exbig_loop
exbig_exit:

;-----------------------------------------------------------------------------
;	Filename���̃p�X���𔲂���filename_ofs2��ݒ�(File����r�p)
;-----------------------------------------------------------------------------
	lds	si,dword ptr cs:[filename_ofs]
	mov	bx,si
yencheck_loop:
	lodsb
	call	sjis_check	;in DISKPMD.INC
	jnc	yencheck_nonsjis
	inc	si	;2byte�ڂ��΂�
	jmp	yencheck_loop
yencheck_nonsjis:
	or	al,al
	jz	yencheck_exit
	cmp	al,"\"
	jnz	yencheck_loop
	mov	bx,si
	jmp	yencheck_loop
yencheck_exit:
	mov	cs:[filename_ofs2],bx
	mov	ax,ds
	mov	cs:[filename_seg2],ax	;�Z�O�����g��filename_seg�ƈꏏ

	ret

filename_set	endp

;==============================================================================
;	PMD�̃��[�N��Filename������
;==============================================================================
write_filename_to_pmdwork	proc
	lds	si,dword ptr cs:[filename_ofs2]
	les	di,dword ptr cs:[pcmwork_ofs]
	add	di,4*256+2	;ES:DI = PMD��PCM_WORK��Filename�i�[�ʒu
	mov	cx,128		;byte��
write_filename_loop:
	dec	cx
	movsb
	cmp	byte ptr -1[si],0
	jnz	write_filename_loop
	xor	al,al
rep	stosb			;�c����O�Ŗ��߂�
	ret
write_filename_to_pmdwork	endp

;==============================================================================
;	�o�b�l�������փ��C������������f�[�^�𑗂� (x8,����/�ᑬ�I���)
;
;	INPUTS 	.. cs:[pcmstart]   	to Start Address
;		.. cs:[pcmstop]    	to Stop  Address
;		.. cs:[pcmdata_ofs/seg]	to PCMData_Buffer
;==============================================================================
pcmstore	proc
	call	key_check_reset

	mov	dx,0001h
	call	out46

	mov	dx,1017h	;brdy�ȊO�̓}�X�N(=timer���荞�݂͊|����Ȃ�)
	call	out46
	mov	dx,1080h
	call	out46
	mov	dx,0060h
	call	out46
	mov	dx,0102h	;x8
	call	out46

	mov	dx,0cffh
	call	out46
	inc	dh
	call	out46

	mov	bx,cs:[pcmstart]
	mov	dh,002h
	mov	dl,bl
	call	out46
	inc	dh
	mov	dl,bh
	call	out46

	mov	dx,04ffh
	call	out46
	inc	dh
	call	out46

	lds	si,dword ptr cs:[pcmdata_ofs]
	mov	cx,cs:[pcmstop]
	sub	cx,cs:[pcmstart]
	add	cx,cx
	add	cx,cx
	add	cx,cx
	add	cx,cx
	add	cx,cx

	mov	dx,cs:[port46]
	mov	bx,cs:[port47]

	cmp	cs:[adpcm_wait],0
	jz	fast_store
	cmp	cs:[adpcm_wait],1
	jz	middle_store

;------------------------------------------------------------------------------
;	�ᑬ��`
;------------------------------------------------------------------------------
slow_store:
	cli
o4600z:	in	al,dx
	or	al,al
	js	o4600z

	mov	al,8	;PCMDAT	reg.
	out	dx,al
	push	cx
	mov	cx,cs:[wait_clock]
	loop	$
	pop	cx

	xchg	bx,dx
	lodsb
	out	dx,al	;OUT	data
	sti
	xchg	dx,bx
o4601z:
	in	al,dx
	test	al,8	;BRDY	check
	jz	o4601z
o4601zb:
	test	al,al	;BUSY	check
	jns	o4601zc
	in	al,dx
	jmp	o4601zb
o4601zc:

	mov	al,10h
	cli
	out	dx,al
	push	cx
	mov	cx,cs:[wait_clock]
	loop	$
	pop	cx
	xchg	dx,bx
	mov	al,80h
	out	dx,al	;BRDY	reset
	sti
	xchg	dx,bx

	loop	slow_store

	jmp	pcmst_exit

;------------------------------------------------------------------------------
;	������`
;------------------------------------------------------------------------------
middle_store:
	call	cli_sub
o4600y:	in	al,dx
	or	al,al
	js	o4600y
	mov	al,8	;PCMDAT	reg.
	out	dx,al

middle_store_loop:
	push	cx
	mov	cx,cs:[wait_clock]
	loop	$
	pop	cx

	xchg	bx,dx
	lodsb
	out	dx,al	;OUT	data
	xchg	bx,dx
o4601y:
	in	al,dx
	test	al,8	;BRDY	check
	jz	o4601y

	loop	middle_store_loop
	call	sti_sub

	jmp	pcmst_exit

;------------------------------------------------------------------------------
;	������`
;------------------------------------------------------------------------------
fast_store:
	call	cli_sub
o4600x:	in	al,dx
	or	al,al
	js	o4600x
	mov	al,8	;PCMDAT	reg.
	out	dx,al
	push	cx
	mov	cx,cs:[wait_clock]
	loop	$
	pop	cx
	xchg	bx,dx

fast_store_loop:
	lodsb
	out	dx,al	;OUT	data
	xchg	bx,dx
o4601x:
	in	al,dx
	test	al,8	;BRDY	check
	jz	o4601x

	xchg	dx,bx
	loop	fast_store_loop
	call	sti_sub

pcmst_exit:
	mov	dx,1000h
	call	out46
	mov	dx,1080h
	call	out46
	mov	dx,0001h
	call	out46

	call	key_check_set
	ret

;------------------------------------------------------------------------------
;	RS-232C�ȊO�͊��荞�݂��֎~����
;	(FM����LSI �� ADDRESS�̕ύX�������Ȃ���)
;------------------------------------------------------------------------------
cli_sub:
	push	ax
	push	dx
	cli
	mov	dx,cs:[mmask_port]
	in	al,dx
	mov	cs:[mmask_push],al
	or	al,11101111b		;RS�̂ݕω������Ȃ�
	out	dx,al
	sti
	pop	dx
	pop	ax
	ret

;------------------------------------------------------------------------------
;	���subroutine�ŋ֎~�������荞�݂����ɖ߂�
;------------------------------------------------------------------------------
sti_sub:
	push	ax
	push	dx
	cli
	mov	dx,cs:[mmask_port]
	mov	al,cs:[mmask_push]
	out	dx,al
	sti
	pop	dx
	pop	ax
	ret

pcmstore	endp

;==============================================================================
;	�o�b�l���������烁�C���������ւ̃f�[�^��荞��
;
;	Inputs	.. cs:[pcmstart]  	to Start Address
;		.. cs:[pcmstop]   	to Stop  Address
;		.. cs:[pcmdata_ofs/seg]	to PCMDATA_Buffer
;==============================================================================
pcmread	proc
	call	key_check_reset
	jnc	pcmread_exec
	ret

pcmread_exec:
	mov	dx,0001h
	call	out46

	mov	dx,1000h
	call	out46
	mov	dx,1080h
	call	out46
	mov	dx,0020h
	call	out46
	mov	dx,0102h	;x8
	call	out46
	mov	dx,0cffh
	call	out46
	inc	dh
	call	out46
	mov	bx,cs:[pcmstart]
	mov	dh,002h
	mov	dl,bl
	call	out46
	mov	dh,003h
	mov	dl,bh
	call	out46
	mov	dx,04ffh
	call	out46
	inc	dh
	call	out46

	call	pget
	call	pget

	mov	cx,cs:[pcmstop]
	sub	cx,cs:[pcmstart]
	add	cx,cx
	add	cx,cx
	add	cx,cx
	add	cx,cx
	add	cx,cx

	les	di,dword ptr cs:[pcmdata_ofs]

pcr00:	mov	dx,cs:[port46]
	cli
pcr00b:	in	al,dx
	test	al,al
	js	pcr00b
	mov	al,8
	out	dx,al

pcr01:	in	al,dx
	test	al,00001000b
	jz	pcr01

pcr02:	in	al,dx
	or	al,al
	js	pcr02

	mov	dx,cs:[port47]
	in	al,dx
	sti
	stosb

	mov	dx,1080h
	call	out46

	loop	pcr00

	mov	dx,0001h
	call	out46

	call	key_check_set
	ret

;==============================================================================
;	���ʓǂݗp
;==============================================================================
pget:	cli
	mov	dx,cs:[port46]
pg00:	in	al,dx
	test	al,al
	js	pg00
	mov	al,008h
	out	dx,al

	push	cx
	mov	cx,cs:[wait_clock]
	loop	$
	pop	cx

	mov	dx,cs:[port47]
	in	al,dx
	sti
	mov	dx,1080h
	call	out46
	ret

pcmread	endp

;==============================================================================
;	�n�o�m�`���|�[�g�ւ̃f�[�^�̏�������
;
;	Inputs	.. dh to Register
;		.. dl to Data
;==============================================================================
out46	proc
	push	dx
	push	bx
	mov	bx,dx
	mov	dx,cs:[port46]
o4600:	in	al,dx
	or	al,al
	js	o4600
	mov	al,bh
	cli
	out	dx,al
	push	cx
	mov	cx,cs:[wait_clock]
	loop	$
	pop	cx
	mov	dx,cs:[port47]
	mov	al,bl
	out	dx,al
	sti
	pop	bx
	pop	dx
	ret

out46	endp

;==============================================================================
;	PMD�� ESC/GRPH���͂������Ȃ�����
;	���̑��K�v�ȃf�[�^��pmd��segment����ǂݎ��
;		out	cy	acccess flag on
;==============================================================================
key_check_reset:
	push	ds
	push	ax
	push	bx
	push	dx
	push	di
	mov	ah,10h
	int	60h
	mov	bx,dx
	mov	di,18[bx]
	mov	bx,-2[bx]
	mov	ax,_wait_clock[bx]
	mov	cs:[wait_clock],ax	;get wait_clock
	mov	al,_adpcm_wait[bx]
	mov	cs:[adpcm_wait],al	;get adpcm_wait
	mov	cs:[mmask_port],02h	;master_mask (98)
	cmp	word ptr ds:[84h],"AV"
	jnz	kcr_notva
	mov	cs:[mmask_port],18ah	;master_mask (VA)
kcr_notva:
	cmp	_pcm_access[bx],0	;cf=0
	jnz	kcr_exit
	mov	al,_key_check[bx]
	mov	cs:[key_check_push],al
	mov	_key_check[bx],0
	mov	_pcm_access[bx],1
	mov	_pcmflag[bx],0		;�ǉ� (���ʉ��΍�)
	mov	_pcm_effec_num[bx],255	;//
	and	_partmask[di],0fdh	;// bit1��clear
	stc				;cf=1
kcr_exit:
	cmc
	pop	di
	pop	dx
	pop	bx
	pop	ax
	pop	ds
	ret

;==============================================================================
;	PMD�� ESC/GRPH���͂����ɖ߂�
;	PCM�������A�N�Z�X�t���O��off
;==============================================================================
key_check_set:
	push	ds
	push	ax
	push	bx
	push	dx
	mov	ah,10h
	int	60h
	mov	bx,dx
	mov	bx,-2[bx]
	mov	al,cs:[key_check_push]
	mov	_key_check[bx],al
	mov	_pcm_access[bx],0
	pop	dx
	pop	bx
	pop	ax
	pop	ds
	ret

;==============================================================================
;	�g���q��ݒ�
;		in.	cs:[filename_ofs]	filename
;			cs:bx			�g���q(3����)
;		out.	cs:filename_buf		�g���q��ύX����file��
;==============================================================================
ext_set:
	mov	ax,cs
	mov	es,ax				;es:bx = �g���q
	lds	si,dword ptr cs:[filename_ofs]	;ds:si = file���擪
	mov	di,offset filename_buf		;es:di = filename_buf�擪

;-----------------------------------------------------------------------------
;	Filename���̊g���q�̏ꏊ��T�� & filename_buf�ɓ]��
;-----------------------------------------------------------------------------
	xor	dx,dx
extcheck_loop:
	lodsb
	stosb
	call	sjis_check	;in DISKPMD.INC
	jnc	extcheck_nonsjis
	movsb			;2byte�ڂ��΂�
	jmp	extcheck_loop
extcheck_nonsjis:
	or	al,al
	jz	extcheck_exit
	cmp	al,"."
	jnz	extcheck_loop
	mov	dx,di
	jmp	extcheck_loop
extcheck_exit:

;-----------------------------------------------------------------------------
;	�g���q��ύX
;-----------------------------------------------------------------------------
	or	dx,dx
	jnz	ext_found
	mov	dx,di
	mov	byte ptr es:-1[di],"."	;"."���Ȃ� = �ŏI�ʒu
ext_found:
	mov	di,dx		;es:di = �ύX�����g���q�̈ʒu
	mov	ax,cs
	mov	ds,ax
	mov	si,bx		;ds:si = �ύX�����g���q�̈ʒu

	movsw
	movsb			;�g���q�ύX

	ret

;==============================================================================
;	DataArea
;==============================================================================
if	message
allload_mes	db	"�o�b�l���`���ł��B���΂炭���҂��������B",13,10,"$"
exit1_mes	db	"PCM����`�o������ł͂���܂���B",13,10,"$"
exit1p_mes	db	"PPSDRV���풓���Ă��܂���B",13,10,"$"
exit2_mes	db	"PCMFile��������܂���B",13,10,"$"
exit2p_mes	db	"PPSFile��������܂���B",13,10,"$"
exit3_mes	db	"PCMFile��FORMAT���Ⴂ�܂��B",13,10,"$"
exit3p_mes	db	"PPSDRV�̊m�ۗe�ʂ�����܂���B",13,10,"$"
exit4_mes	db	"PCMData����v�����̂œǂݍ��݂܂���B",13,10,"$"
exit4pp_mes	db	"P86DRV�̊m�ۗe�ʂ�����܂���B",13,10,"$"
exit5_mes	db	"PCMFile���ǂݍ��߂܂���B",13,10,"$"
exit5p_mes	db	"PPSFile���ǂݍ��߂܂���B",13,10,"$"
exit6_mes	db	"PCM�������𑼂̃A�v���P�[�V�������A�N�Z�X���ł��B",13,10,"$"
exit1z_mes	db	"PCMFile��������܂���B",13,10,"$"
exit2z_mes	db	"PCMFile�̃f�[�^�`�����Ⴂ�܂��B",13,10,"$"
exit3z_mes	db	"�������m�ۗe�ʂ�����܂���B",13,10,"$"
exit4z_mes	db	"EMS�n���h���̃}�b�s���O���ł��܂���B",13,10,"$"
exit5z_mes	db	"PPZ8���풓���Ă��܂���B",13,10,"$"
ppzbank_mes	db	"PPZ8("
banknum		db	"?):$"
endif
adpcm_header	db	"ADPCM DATA for  PMD ver.4.4-  " ;30 bytes
pps_ext		db	"PPS"
ppc_ext		db	"PPC"
p86_ext		db	"P86"
pvi_ext		db	"PVI"
pzi_ext		db	"PZI"

retry_flag	db	0
key_check_push	db	0
wait_clock	dw	0
adpcm_wait	db	0
mmask_port	dw	0
mmask_push	db	0

filename_buf	db	128 dup(?)

		even
filename_ofs	dw	?
filename_seg	dw	?
filename_ofs2	dw	?
filename_seg2	dw	?
pcmdata_ofs	dw	?
pcmdata_seg	dw	?
pcmdata_size_s	db	?
pcmdata_size	dw	?
pcmwork_ofs	dw	?
pcmwork_seg	dw	?
port46		dw	?
port47		dw	?
pcmstop		dw	?
pcmstart	dw	?
fhand2		dw	?
ppz_bank	db	?

open_work	struc
_mmlbuf		dw	?		;Musicdata��address+1
_tondat		dw	?		;Voicedata��address
_efcdat		dw	?		;FM  Effecdata��address
_fm1_port1	dw	?		;FM���� I/O port (�\1)
_fm1_port2	dw	?		;FM���� I/O port (�\2)
_fm2_port1	dw	?		;FM���� I/O port (��1)
_fm2_port2	dw	?		;FM���� I/O port (��2)
_fmint_ofs	dw	?		;FM���荞�݃t�b�N�A�h���X offset
_fmint_seg	dw	?		;FM���荞�݃t�b�N�A�h���X address
_efcint_ofs	dw	?		;���ʉ����荞�݃t�b�N�A�h���X offset
_efcint_seg	dw	?		;���ʉ����荞�݃t�b�N�A�h���X address
_prgdat_adr	dw	?		;�ȃf�[�^�����F�f�[�^�擪�Ԓn
_radtbl		dw	?		;R part offset table �擪�Ԓn
_rhyadr		dw	?		;R part ���t���Ԓn
_rhythmmask	db	?		;Rhythm�����̃}�X�N x8c/10h��bit�ɑΉ�
_board		db	?		;FM�����{�[�h����^�Ȃ�flag
_key_check	db	?		;ESC/GRPH key Check flag
_fm_voldown	db	?		;FM voldown ���l
_psg_voldown	db	?		;PSG voldown ���l
_pcm_voldown	db	?		;PCM voldown ���l
_rhythm_voldown	db	?		;RHYTHM voldown ���l
_prg_flg	db	?		;�ȃf�[�^�ɉ��F���܂܂�Ă��邩flag
_x68_flg	db	?		;OPM flag
_status		db	?		;status1
_status2	db	?		;status2
_tempo_d	db	?		;tempo (TIMER-B)
_fadeout_speed	db	?		;Fadeout���x
_fadeout_volume	db	?		;Fadeout����
_tempo_d_push	db	?		;tempo (TIMER-B) / �ۑ��p
_syousetu_lng	db	?		;���߂̒���
_opncount	db	?		;�ŒZ�����J�E���^
_TimerAtime	db	?		;TimerA�J�E���^
_effflag	db	?		;PSG���ʉ�����on/off flag
_psnoi		db	?		;PSG noise���g��
_psnoi_last	db	?		;PSG noise���g��(�Ō�ɒ�`�������l)
_fm_effec_num	db	?		;��������FM���ʉ��ԍ�
_fm_effec_flag	db	?		;FM���ʉ�������flag (1)
_disint		db	?		;FM���荞�ݒ��Ɋ��荞�݂��֎~���邩flag
_pcmflag	db	?		;PCM���ʉ�������flag
_pcmstart	dw	?		;PCM���F��start�l
_pcmstop	dw	?		;PCM���F��stop�l
_pcm_effec_num	db	?		;��������PCM���ʉ��ԍ�
__pcmstart	dw	?		;PCM���ʉ���start�l
__pcmstop	dw	?		;PCM���ʉ���stop�l
__voice_delta_n	dw	?		;PCM���ʉ���delta_n�l
__pcmpan	db	?		;PCM���ʉ���pan
__pcm_volume	db	?		;PCM���ʉ���volume
_rshot_dat	db	?		;���Y������ shot flag
_rdat		db	6 dup (?)	;���Y������ ����/�p���f�[�^
_rhyvol		db	?		;���Y���g�[�^�����x��
_kshot_dat	dw	?		;�r�r�f���Y�� shot flag
_ssgefcdat	dw	?		;PSG Effecdata��address
_ssgefclen	dw	?		;PSG Effecdata�̒���
_play_flag	db	?		;play flag
_pause_flag	db	?		;pause flag
_fade_stop_flag	db	?		;Fadeout�� MSTOP���邩�ǂ����̃t���O
_kp_rhythm_flag	db	?		;K/Rpart��Rhythm������炷��flag
_TimerBflag	db	?		;TimerB���荞�ݒ��H�t���O
_TimerAflag	db	?		;TimerA���荞�ݒ��H�t���O
_int60flag	db	?		;INT60H���荞�ݒ��H�t���O
_int60_result	db	?		;INT60H�̎��sErrorFlag
_pcm_gs_flag	db	?		;ADPCM�g�p ���t���O (0�ŋ���)
_esc_sp_key	db	?		;ESC +?? Key Code
_grph_sp_key	db	?		;GRPH+?? Key Code
_rescut_cant	db	?		;�풓�����֎~�t���O
_slot_detune1	dw	?		;FM3 Slot Detune�l slot1
_slot_detune2	dw	?		;FM3 Slot Detune�l slot2
_slot_detune3	dw	?		;FM3 Slot Detune�l slot3
_slot_detune4	dw	?		;FM3 Slot Detune�l slot4
_wait_clock	dw	?		;FM ADDRESS-DATA�� Loop $�̉�
_wait1_clock	dw	?		;loop $ �P�̑��x
_ff_tempo	db	?		;�����莞��TimerB�l
_pcm_access	db	?		;PCM�Z�b�g���� 1
_TimerB_speed	db	?		;TimerB�̌��ݒl(=ff_tempo�Ȃ�ff��)
_fadeout_flag	db	?		;��������fout���Ăяo������1
_adpcm_wait	db	?		;ADPCM��`�̑��x
_revpan		db	?		;PCM86�t��flag
_pcm86_vol	db	?		;PCM86�̉��ʂ�SPB�ɍ��킹�邩?
_syousetu	dw	?		;���߃J�E���^
_int5_flag	db	?		;FM�������荞�ݒ��H�t���O
_port22h	db	?		;OPN-PORT 22H �ɍŌ�ɏo�͂����l(hlfo)
_tempo_48	db	?		;���݂̃e���|(clock=48 t�̒l)
_tempo_48_push	db	?		;���݂̃e���|(����/�ۑ��p)
_rew_sp_key	db	?		;GRPH+?? (rew) Key Code
_intfook_flag	db	?		;int_fook��flag B0:TB B1:TA
_skip_flag	db	?		;normal:0 �O��SKIP��:1 ���SKIP��:2
__fm_voldown	db	?		;FM voldown ���l (�ۑ��p)
__ssg_voldown	db	?		;PSG voldown ���l (�ۑ��p)
__pcm_voldown	db	?		;PCM voldown ���l (�ۑ��p)
__rhythm_voldown db	?		;RHYTHM voldown ���l (�ۑ��p)
__pcm86_vol	db	?		;PCM86�̉��ʂ�SPB�ɍ��킹�邩? (�ۑ��p)
_mstart_flag	db	?		;mstart���鎞�ɂP�ɂ��邾����flag
_mus_filename	db	13 dup(?)	;�Ȃ�FILE���o�b�t�@
_mmldat_lng	db	?		;�ȃf�[�^�o�b�t�@�T�C�Y(KB)
_voicedat_lng	db	?		;���F�f�[�^�o�b�t�@�T�C�Y(KB)
_effecdat_lng	db	?		;���ʉ��f�[�^�o�b�t�@�T�C�Y(KB)
_rshot_bd	db	?		;���Y������ shot inc flag (BD)
_rshot_sd	db	?		;���Y������ shot inc flag (SD)
_rshot_sym	db	?		;���Y������ shot inc flag (CYM)
_rshot_hh	db	?		;���Y������ shot inc flag (HH)
_rshot_tom	db	?		;���Y������ shot inc flag (TOM)
_rshot_rim	db	?		;���Y������ shot inc flag (RIM)
_rdump_bd	db	?		;���Y������ dump inc flag (BD)
_rdump_sd	db	?		;���Y������ dump inc flag (SD)
_rdump_sym	db	?		;���Y������ dump inc flag (CYM)
_rdump_hh	db	?		;���Y������ dump inc flag (HH)
_rdump_tom	db	?		;���Y������ dump inc flag (TOM)
_rdump_rim	db	?		;���Y������ dump inc flag (RIM)
_ch3mode	db	?		;ch3 Mode
_ch3mode_push	db	?		;ch3 Mode(���ʉ��������ppush�̈�)
_ppz_voldown	db	?		;PPZ8 voldown ���l
__ppz_voldown	db	?		;PPZ8 voldown ���l (�ۑ��p)
_ppz_call_ofs	dw	?		;PPZ8call�p far call address
_ppz_call_seg	dw	?		;seg�l��PPZ8�풓check�����˂�,0�Ŕ�풓
_p86_freq	db	?		;PMD86��PCM�Đ����g��
_p86_freqtable	dw	?		;PMD86��PCM�Đ����g��table�ʒu
open_work	ends

part_work	struc
_address	dw	?	; 2 �ݿ���� � ���ڽ
_partloop	dw	?       ; 2 �ݿ� �� �ܯ�ķ � ���ػ�
_leng		db	?       ; 1 ɺ� LENGTH
_qdat		db	?       ; 1 gatetime (q/Q�l���v�Z�����l)
_fnum		dw	?       ; 2 �ݿ���� � BLOCK/FNUM
_detune		dw	?       ; 2 ������
_lfodat		dw	?       ; 2 LFO DATA
_porta_num	dw	?	; 2 �|���^�����g�̉����l�i�S�́j
_porta_num2	dw	?	; 2 �|���^�����g�̉����l�i���j
_porta_num3	dw	?	; 2 �|���^�����g�̉����l�i�]��j
_volume		db	?       ; 1 VOLUME
_shift		db	?       ; 1 �ݶ� ��� � ���
_delay		db	?       ; 1 LFO	[DELAY] 
_speed		db	?       ; 1	[SPEED]
_step		db	?       ; 1	[STEP]
_time		db	?       ; 1	[TIME]
_delay2		db	?       ; 1	[DELAY_2]
_speed2		db	?       ; 1	[SPEED_2]
_step2		db	?       ; 1	[STEP_2]
_time2		db	?       ; 1	[TIME_2]
_lfoswi		db	?       ; 1 LFOSW. B0/tone B1/vol B2/���� B3/porta
				;          B4/tone B5/vol B6/����
_volpush	db	? 	; 1 Volume PUSHarea
_mdepth		db	?	; 1 M depth
_mdspd		db	?	; 1 M speed
_mdspd2		db	?	; 1 M speed_2
_envf		db	?       ; 1 PSG ENV. [START_FLAG] / -1��extend
_eenv_count	db	?	; 1 ExtendPSGenv/No=0 AR=1 DR=2 SR=3 RR=4
_eenv_ar	db	?	; 1 		/AR		/��pat
_eenv_dr	db	?	; 1		/DR		/��pv2
_eenv_sr	db	?	; 1		/SR		/��pr1
_eenv_rr	db	?	; 1		/RR		/��pr2
_eenv_sl	db	?	; 1		/SL
_eenv_al	db	?	; 1		/AL
_eenv_arc	db	?	; 1		/AR�̃J�E���^	/��patb
_eenv_drc	db	?	; 1		/DR�̃J�E���^
_eenv_src	db	?	; 1		/SR�̃J�E���^	/��pr1b
_eenv_rrc	db	?	; 1		/RR�̃J�E���^	/��pr2b
_eenv_volume	db	?	; 1		/Volume�l(0�`15)/��penv
_extendmode	db	?	; 1 B1/Detune B2/LFO B3/Env Normal/Extend
_fmpan		db	? 	; 1 FM Panning + AMD + PMD
_psgpat		db	?       ; 1 PSG PATTERN [TONE/NOISE/MIX]
_voicenum	db	?	; 1 ���F�ԍ�
_loopcheck	db	?	; 1 ���[�v������P �I��������R
_carrier	db	?	; 1 FM Carrier
_slot1		db	?       ; 1 SLOT 1 � TL
_slot3		db	?       ; 1 SLOT 3 � TL
_slot2		db	?       ; 1 SLOT 2 � TL
_slot4		db	?       ; 1 SLOT 4 � TL
_slotmask	db	?	; 1 FM slotmask
_neiromask	db	?	; 1 FM ���F��`�pmaskdata
_lfo_wave	db	?	; 1 LFO�̔g�`
_partmask	db	?	; 1 PartMask b0:�ʏ� b1:���ʉ� b2:NECPCM�p
				;   b3:none b4:none b5:s0�� b6:m b7:�ꎞ
_keyoff_flag	db	?	; 1 Keyoff�������ǂ�����Flag
_volmask	db	?	; 1 ����LFO�̃}�X�N
_qdata		db	?	; 1 q�̒l
_qdatb		db	?	; 1 Q�̒l
_hldelay	db	?	; 1 HardLFO delay
_hldelay_c	db	?	; 1 HardLFO delay Counter
__lfodat	dw	?       ; 2 LFO DATA
__delay		db	?       ; 1 LFO	[DELAY] 
__speed		db	?       ; 1	[SPEED]
__step		db	?       ; 1	[STEP]
__time		db	?       ; 1	[TIME]
__delay2	db	?       ; 1	[DELAY_2]
__speed2	db	?       ; 1	[SPEED_2]
__step2		db	?       ; 1	[STEP_2]
__time2		db	?       ; 1	[TIME_2]
__mdepth	db	?	; 1 M depth
__mdspd		db	?	; 1 M speed
__mdspd2	db	?	; 1 M speed_2
__lfo_wave	db	?	; 1 LFO�̔g�`
__volmask	db	?	; 1 ����LFO�̃}�X�N
_mdc		db	?	; 1 M depth Counter (�ϓ��l)
_mdc2		db	?	; 1 M depth Counter
__mdc		db	?	; 1 M depth Counter (�ϓ��l)
__mdc2		db	?	; 1 M depth Counter
_onkai		db	?	; 1 ���t���̉��K�f�[�^ (0ffh:rest)
_sdelay		db	?	; 1 Slot delay
_sdelay_c	db	?	; 1 Slot delay counter
_sdelay_m	db	?	; 1 Slot delay Mask
_alg_fb		db	?	; 1 ���F��alg/fb
_keyon_flag	db	?	; 1 �V���K/�x���f�[�^������������inc
_qdat2		db	?	; 1 q �Œ�ۏؒl
_fnum2		dw	?	; 2 ppz8/pmd86�pfnum�l���
_onkai_def	db	?	; 1 ���t���̉��K�f�[�^ (�]�������O / ?fh:rest)
_shift_def	db	?	; 1 �}�X�^�[�]���l
_qdat3		db	?	; 1 q Random
		db	?	; dummy
part_work	ends
