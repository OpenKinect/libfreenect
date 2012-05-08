RSRC
 LVCCLBVW  �  �      p      libfreenect.lvlib     x`�             < � @�      ����            �	�.fMM��v6��8          &@�t�F�q6P�jpr��ُ ��	���B~  @                           j LVCC+libfreenect.lvlib:freenect_video_format.ctl      VILB      PTH0       libfreenect.lvlib               "    0     0    0       N  @x�c`�	X8�4L�?@ę�4n���l|b�j�;�V
@(F�.�7@331������ +fB� r�'     : VIDS+libfreenect.lvlib:freenect_video_format.ctl            �  x��a``�4�0;�� ��)� �w��g�
x�;�Y�ݮ�zST8lA�J�w����?�0aB|\Tdt��	��9*��Aa��%**�5*������[~��T]�
�T�
�a�Tʰ#d���0���L�d�t�� i��d�a�H�?��X�f1,bX�����+�W�~ �+@p   `�  8.6    �  �/// Enumeration of video frame information states.
/// See http://openkinect.org/wiki/Protocol_Documentation#RGB_Camera for more information.
typedef enum {
	FREENECT_VIDEO_RGB             = 0, /**< Decompressed RGB mode (demosaicing done by libfreenect) */
	FREENECT_VIDEO_BAYER           = 1, /**< Bayer compressed mode (raw information from camera) */
	FREENECT_VIDEO_IR_8BIT         = 2, /**< 8-bit IR mode  */
	FREENECT_VIDEO_IR_10BIT        = 3, /**< 10-bit IR mode */
	FREENECT_VIDEO_IR_10BIT_PACKED = 4, /**< 10-bit packed IR mode */
	FREENECT_VIDEO_YUV_RGB         = 5, /**< YUV RGB mode */
	FREENECT_VIDEO_YUV_RAW         = 6, /**< YUV Raw mode */
} freenect_video_format;     ������   �  �{��Z��{�ϑZ��z�  �����  � � ���3�R*���3+��3+�  �  �  �  �  �  ;�  G�  �  �  �  #�  C�  ��������   �����������������              �              ������������������������������f��������������f��������������f�����������������������������f�ffffffffffffff�              � �        f�  �        f�����������o�� � ��������� � ��������� � ��������              f�              f�              f�              f�              f�            ��f�            f�             f�             �f�             f�            � f�             f�           ��f��ffffffffffffff�fffffffffffffff   ���������������������������������                              +� $$$$$$$$$$$$$$$$$$$$$$$$$$$$+�� $������������������������������ $���������ö��������ö��������� $���������ö��������ʶ��������� $���������ö������������������� $������������������������������ $������������������������������ N������������������������������ $$$$$$$$$$$$$$$$$$$$$$$$$$$$N�� $�N�N�NNNy�NNNNNNNNNNNNNNNNN��� $�NNN�NNN�ONNNNNNNNNNNNNNNNN��� $�N�N��yN��N���Ny�ONy�ON��yN��� $�N�N�y�N�NN�NNN�y�N�y�N�O�N��� $�N�N��yN�NN�NNN��yN��yN�N�N��� $�N�N��yN�NN�NNN��yN��yN�N�N��� $NNNNNNNNNNNNNNNNNNNNNNNNNNN��� $NNNNNNNNNNNNNNNNNNNNNNNNNNN��� $NNNNNNNNNNNNNNNNNNNNNNNNNNN��� $NNNNNNNNNNNNNNNNNNNNNNNNNNN��� $NNNNNNNNNNNNNNNNNNNNNNNNNNN��� $NNNNNNNNNNNNNNNNNNNNNNN���N��� $NNNNNNNNNNNNNNNNNNNNNN�NNN���� $NNNNNNNNNNNNNNNNNNNNNNNNNN���� $NNNNNNNNNNNNNNNNNNNNNNNNN�N��� $NNNNNNNNNNNNNNNNNNNNNNNN�NN��� $NNNNNNNNNNNNNNNNNNNNNNN�NNN��� $NNNNNNNNNNNNNNNNNNNNNN�NNNN��� $NNNNNNNNNNNNNNNNNNNNNN�������� +������������������������������+������������������������������              : FPHP+libfreenect.lvlib:freenect_video_format.ctl           �  �x��T�OA~�luh�E6���)`bI�`�')��*A䇈�h�����E9��L��j��b��{�(��� ��[�-�$�3����{o��W �;V���,˶q�ΥV	�e���j��� [W\Ҁ�&3ga�=����Z�:w��g}�#�r�~�9{��n�%.3��v�wd"ms�jQjH-k����ӌ2��SIP�=�&������Gܹ(��E�p_����!�OJ���#���܉S�n�~����@��I�G�&	U9��\֏oCh�*���=�j�z��J�
�U�C0��<6�=ߤ��K����Nڀ�f�TW��Ƨ����t���Tz29>9�`dڳ-?�X���[�7y]֚�d�֝��]}Y�(-jOGZc�-?���q�RJ�st��Z�Zi}S��gY=V�@�v´n&��ū=P�c��񀉾d{<1P{�C��1x����ezՆ�"�p�Pa��b����ȵ��6\����G=��@aq˂j��HH�,\8�c4,�������� C�O��ɄA��	GOL8z ����;��tS��'i�D�!r��@Z4�F�Y�[>�i.�f�{��2]g~�<gvV-�����,��]�%[*�B�b��D�t�n�_�n{^��:@�B����l o�/�HD�F e���.��e�u��Ź-�	�;\�o�_y-�9p���j0��j8��
� �x�g�?g�c����D�x�bͥ+���            r   : BDHP+libfreenect.lvlib:freenect_video_format.ctl            b   wx�c``(�`��P�W�+���!�������O�0�f�
���j�課1�X1sHr� �q�h0���_����+�5�Y����� {�          z      G   (                                        �                   8�    @UUA N 	                                                   ���=  �>���=  �>    @   ?          R  ]x�uPIO�`}](�"�+�\=���)�OӘ(iPé��	)����������.�}I���̛��^�2҅SK�����fG\�֙Q\��V_X�Ѵ�a�*��F�z����碳�ֿ2��ʭ�����A`?>8�o{�dt`	@z��0t 4 #�[�*]F
�R�!!���x�{d�'ŷ�Xp}y���4�����Р8��`��w��	݁e�[�W��ѡU>B��tH��ؑ�K�P�x:��*&o��,|�BϘ'q��L�^>�C�2���*N蹋=��@��3���E��Jا�|��sXGlb�����%> t_�     e       H      � �   Q      � �   Z      � �   c� � �   � �Segoe UISegoe UISegoe UI0   RSRC
 LVCCLBVW  �  �      p               4  �   LIBN      HLVSR      \RTSG      pLIvi      �CONP      �TM80      �DFDS      �LIds      �VICD      �vers      �STRG      ICON      $icl4      8icl8      LCPC2      `DTHP      tLIfp      �FPHc      �FPSE      �LIbd      �BDHc      �BDSE      �MUID       HIST      PRT       (VCTP      <FTAB      P    ����                                   ����       �        ����       �        ����              ����      $        ����      L        ����      �        ����      �       ����      �        ����      �        ����      `        ����      �        ����      �        ����      �        ����      �        ����      �        ����      <        ����      4        ����      @        ����      �        ����      �        ����      �        ����      �        ����      (        ����      �       �����          freenect_video_format.ctl