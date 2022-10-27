import sigrokdecode as srd

'''
'''

class ChannelError(Exception):
    pass

class Decoder(srd.Decoder):
    api_version = 3
    id = 'C2'
    name = 'C2 interface'
    longname = 'Silabs C2 Interface'
    desc = 'Half-duplex, synchronous, serial bus.'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = ['C2']
    tags = ['Embedded/mcu']
    channels = (
        {'id': 'c2ck', 'type': 0, 'name': 'c2ck', 'desc': 'Clock', 'idn':'dec_c2_chan_c2ck'},
        {'id': 'c2d', 'type': 0, 'name': 'c2d', 'desc': 'Data', 'idn':'dec_c2_chan_c2d'},
    )
    optional_channels = ()
    annotations = (
        ('106', 'raw-Data', 'raw data'),
        ('106', 'c2-data', 'c2 data'),
        ('warnings', 'Warnings'),
    )
    annotation_rows = (
        ('raw-Data', 'raw data', (0,)),
        ('c2-data', 'c2 data', (1,)),
        ('warnings', 'Warnings', (2,)),
    )

    def __init__(self):
        self.reset()

    def reset(self):
        self.samplerate = None
        self.state= 'reset'
        self.bitcount = 0
        self.c2data =  0
        self.data=0
        self.c2dbits = []
        self.ss_block = -1
        self.samplenum = -1
        self.have_c2ck = self.have_c2d = None
        self.ins= None
        self.dataLen=0
        self.remainData=0

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)

    def metadata(self, key, value):
        if key == srd.SRD_CONF_SAMPLERATE:
            self.samplerate = value

    def decode(self):
        if not self.has_channel(0):
            raise ChannelError('CLK pin required.')
        self.have_c2d = self.has_channel(1)
        if not self.have_c2d:
            raise ChannelError('C2D pins required.')
        tf=0
        tr=0
        while True:
            (c2ck,c2d)=self.wait({0:'e'})
            if c2ck == 0:   #�½���   
                tf=self.samplenum
                if self.state == 'dataRead':
                    if self.bitcount ==0:
                        ss=tr      
                        self.c2data=0
                    self.c2data |= c2d <<self.bitcount
                    self.bitcount += 1
                    if self.bitcount >= 8:
                        self.put(ss, tf, self.out_ann, [0, ['%02X' % self.c2data]])
                        self.bitcount=0    
                        self.data|=self.c2data<<((self.dataLen-self.remainData)*8)
                        self.remainData -= 1
                        if self.remainData ==0:
                            self.state = 'end'
                elif self.state == 'addressRead':
                    if self.bitcount ==0:
                        ss=tr              
                        self.c2data=0
                    self.c2data |= c2d <<self.bitcount
                    self.bitcount += 1
                    if self.bitcount >= 8:
                        self.put(ss, tf, self.out_ann, [0, ['%02X' % self.c2data]])
                        self.state = 'end'
                elif self.state == 'readWait':
                    if self.bitcount ==0:
                        ss=tf    
                    self.bitcount +=1
                    if c2d == 1:
                        self.put(ss, tf, self.out_ann, [0, ['Wait','W']])
                        self.bitcount=0
                        self.state = 'dataRead'
                elif self.state == 'writeWait':
                    if self.bitcount ==0:
                        ss=tr              
                    self.bitcount += 1
                    if c2d == 1:
                        self.put(ss, tf, self.out_ann, [0, ['Wait','W']])
                        self.state = 'end'
                    
            else: #������
                tr=self.samplenum
                interval=(tr-tf)*1000*1000/self.samplerate #us
                if interval>20:
                    self.put(tf, tr, self.out_ann, [0, [ 'Reset','R']])
                    self.state='start'
                elif self.state == 'start':
                    self.put(tf, tr, self.out_ann, [0, [ 'Start','S']])
                    self.state='ins'
                    self.bitcount=0
                    self.ins=0
                    self.data=0
                    self.dataLen=0
                    ss1=tf
                elif self.state == 'ins':
                    if self.bitcount ==0:
                        ss=tr              
                        self.c2data=0
                    self.ins |= c2d <<self.bitcount
                    self.bitcount += 1
                    if self.bitcount >= 2:
                        (c2ck,c2d)=self.wait({0:'f'})
                        if self.ins == 0 :
                            self.state = 'dataReadLen'
                        elif self.ins == 2:
                            self.state = 'addressRead'
                        elif self.ins == 1:
                            self.state = 'dataWriteLen'
                        else:
                            self.state = 'addressWrite'
                        self.put(ss, self.samplenum, self.out_ann, [0, [ '%1d'%self.ins]])
                        self.bitcount=0
                elif self.state == 'addressWrite':
                    if self.bitcount ==0:
                        ss=tr              
                        self.c2data=0
                    self.c2data |= c2d <<self.bitcount
                    self.bitcount += 1
                    if self.bitcount >= 8:
                        (c2ck,c2d)=self.wait({0:'f'})
                        tf=self.samplenum
                        self.put(ss, tf, self.out_ann, [0, ['%02X' % self.c2data]])
                        self.bitcount=0     
                        self.state = 'end'
                elif self.state == 'dataReadLen':
                    if self.bitcount ==0:
                        ss=tr              
                        self.c2data=0
                    self.c2data |= c2d <<self.bitcount
                    self.bitcount += 1
                    if self.bitcount >= 2:
                        self.dataLen=self.c2data+1
                        self.remainData=self.dataLen
                        #(c2ck,c2d)=self.wait({0:'f'})
                        self.put(ss, self.samplenum, self.out_ann, [0, [ '%01d'%self.c2data]])
                        self.state='readWait'
                        self.bitcount=0                         
                elif self.state == 'dataWriteLen':
                    if self.bitcount ==0:
                        ss=tr           
                        self.c2data=0
                    self.c2data |= c2d <<self.bitcount
                    self.bitcount += 1
                    if self.bitcount >= 2:
                        self.dataLen=self.c2data+1
                        self.remainData=self.dataLen
                        (c2ck,c2d)=self.wait({0:'f'})
                        self.put(ss, self.samplenum, self.out_ann, [0, ['%01d'%self.c2data]])
                        self.state='dataWrite'
                        self.bitcount=0 
                        self.c2data=0
                elif self.state == 'dataWrite':
                    if self.bitcount ==0:
                        ss=tr      
                        self.c2data=0
                    self.c2data |= c2d <<self.bitcount
                    self.bitcount += 1
                    if self.bitcount >= 8:
                        self.put(ss, tr, self.out_ann, [0, ['%02X' % self.c2data]])
                        self.bitcount=0    
                        self.data|=self.c2data<<((self.dataLen-self.remainData)*8)
                        self.remainData -= 1
                        if self.remainData ==0:
                            self.state='writeWait'
                elif self.state == 'end':
                    self.state='start'
                    self.put(tf, tr, self.out_ann, [0, [ 'End','E']])
                    if self.ins == 0:
                        self.put(ss1, tr, self.out_ann, [1, [ 'ReadData(%01d)=0x%02X'%(self.dataLen,self.data)]])
                    elif self.ins == 1:
                        self.put(ss1, tr, self.out_ann, [1, [ 'WriteData(0x%02X,%01d)'%(self.data,self.dataLen)]])
                    elif self.ins == 2:
                        self.put(ss1, tr, self.out_ann, [1, [ 'ReadAddress()=0x%02X'%self.c2data]])
                    elif self.ins == 3:
                        self.put(ss1, tr, self.out_ann, [1, [ 'WriteAddress(0x%02X)'%self.c2data]])
                        
