//*****************************************************************************
//* /wfs/ �t�@�C���� : crc_ccitt.h
//* /wss/ ���e : CRC-CCITT�̌v�Z��`
//* /wse/
//* /wns/ �쐬 : ���V
//* /wds/ ���t : 00120100831
//* /wrs/ �X�V : 
//* /wre/
//*****************************************************************************
#ifndef _CRC_CCITT_INCLUDED_
#define _CRC_CCITT_INCLUDED_

/******************************************************************************
�@�C���N���[�h�t�@�C��
******************************************************************************/

/******************************************************************************
�@�萔
******************************************************************************/

/******************************************************************************
�@�^��`
******************************************************************************/

/******************************************************************************
�@�֐��v���g�^�C�v
******************************************************************************/
unsigned short crc_ccitt(unsigned char *pData,	// ���̓f�[�^�ւ̃|�C���^
						 unsigned long iSize,	// ���̓f�[�^��
						 unsigned short init,	// �����l
						 unsigned short xor		// �v�Z���XOR����l
						);						// CRC�v�Z

/******************************************************************************
�@�O���[�o���f�[�^
******************************************************************************/

/******************************************************************************
�@�v���O����
******************************************************************************/

#endif /* _CRC_CCITT_INCLUDED_ */
