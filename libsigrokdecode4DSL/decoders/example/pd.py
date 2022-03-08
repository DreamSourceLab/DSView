##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2011 Gareth McMullin <gareth@blacksphere.co.nz>
## Copyright (C) 2012-2014 Uwe Hermann <uwe@hermann-uwe.de>
## Copyright (C) 2022 DreamSourceLab <support@dreamsourcelab.com>
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, see <http://www.gnu.org/licenses/>.
##

## 开发脚本的前提条件是必须掌握python语言
## 非windows用户，想要打印出信息，用self.printlog(string),并在控制台下运行DSView。但是最终需要注释掉，因为影响性能。
## 更复杂的协议请查考(0-i2c、0-spi、 0-uart、1-i2c、1-spi、1-uart)
## 底层模块提供的属性有：
##  1. samplenum 数据样品位置
##  2. matched  本次查找调用wait匹配的信息，返回一个uint64类型数值，表示０到63个通道的匹配信息
## The prerequisite for developing scripts is to master Python language

## text block fill color table:
##  [#EF2929,#F66A32,#FCAE3E,#FBCA47,#FCE94F,#CDF040,#8AE234,#4EDC44,#55D795,#64D1D2
##  ,#729FCF,#D476C4,#9D79B9,#AD7FA8,#C2629B,#D7476F]

## 导出核心模块类
import sigrokdecode as srd

## 本协议模块具体代码
class Decoder(srd.Decoder):
    api_version = 3

    ## 协议标识，必须唯一
    ## The protocol ID, which must be unique
    id = 'example'

    ## 协议名称, 不一定要求跟标识一致
    ## The protocol name, it not necessarily consistent with ID
    name = 'example'

    ## 协议长名称
    ## Long name
    longname = 'example-devel'

    ## 简介内容
    ## Descript text
    desc = 'This is an example of protocol development'

    ## 开源协议
    license = 'gplv2+'

    ## 接收的输入的数据源名
    ## Ten input data source name
    inputs = ['logic']

    ## 输出的数据类别名，可作为下层协议的输入数据源名
    ## The output data source name
    outputs = ['test']

    ## 适用范围标签
    tags = ['Embedded/industrial']

    ## 必须要绑定的通道定义，将在界面上可见
    ## id:通道标识, 任意命名
    ## type:类型，根据需要设置一个值
    ## name:标签名
    ## desc:该通道的说明
    channels = (
        {'id': 'c1', type:0, 'name': 'c1', 'desc': 'chan1-input'},
    )

    ## 可选通道，其它跟上面的一样
    optional_channels = (
        {'id': 'c2', type:0, 'name': 'c2', 'desc': 'chan２-input'},
    )

    ## 提供给用户通过界面设置的参数，根据业务需要来定义
    options = (
        {'id': 'bit-len', 'desc': 'match bit length', 'default': 16, 'values': (8,16)},
        {'id': 'mode', 'desc': 'work mode', 'default': 'up','values': ('up','low')},
     )

    ##　解析结果定义
    #＃ annotations里的每一项可以有2到3个属性，当有３个属性时，第一个表示类型
    ##  类型对应0-16个颜色，当类型范围在200-299时，将绘制边沿箭头

    annotations = (
        (100, 'test-data1', 'example test data1'),
        (201, 'test-data2', 'example test data2'), 
        (1, 'test-data3', 'example test data3'), 
    )

    ## 解析结果的行定义
    annotation_rows = (
        ('lab1', 'the lab1', (0,)),     #可输出第1个定义的annotations类型
        ('lab2', 'the lab2', (1,2)),    #可输出第1个和第2个定义的annotations类型
    )

    ## 构造函数，自动被调用
    def __init__(self):
        self.reset()

    ## 在这里做一些重置和定义私有变量工作
    def reset(self):
        self.count = 0

    ## 脚本开始运行时，会自动调用
    ## 注册一些解析结果类型
    ## 有: OUTPUT_ANN，OUTPUT_PYTHON，OUTPUT_BINARY，OUTPUT_META
    def start(self):
       self.out_ann = self.register(srd.OUTPUT_ANN)
       #self.out_python = self.register(srd.OUTPUT_PYTHON)

    ## 定义一个输出函数
    ## a,b为采样起点和终点, value为要输出的数值
    def put_row1(self, a, b, value):
        #'@%02X', 前边加@是告诉底层模块这是一个数值数据，显示格式可转化为hex/oct/bin/acii/dec
        # {$}是占位符,内容由数值部分填充
        # @特殊符号和{$}占位符的特性，只有DSView版本在1.2.0以上才支持
        # []描述输出内容，第一个元素表示annotation序号,annotation的行号由annotation_row决定
        self.put(a, b, self.out_ann, [0, ['{$}', '@%02X' % value]])
    
    def put_row2(self, a, b, value):
        self.put(a, b, self.out_ann, [1, ['{$}', '@%02X' % value]])

    def put_row3(self, a, b, value):
        self.put(a, b, self.out_ann, [2, ['row3']])

    ## 触发解析工作
    def decode(self):
        step = 0
        times = 0
        lst_dex = self.samplenum
        b_f = False
        flag_arr = [{0:'r'}, {0:'f'}]
        flag_dex = 0

        while True:
            # 从原始数据中按条件匹配数据，用于接受返回值的元组里的变量个数由通道数决定
            # 如果可选通道未绑定真实通道，则返回255,比如这里的b变成255
            # wait()不带条件，表示进行所有匹配
           #(a,b) = self.wait()
           
           (a,b) = self.wait(flag_arr[flag_dex])
           
           if b_f == False:
               b_f = True
               flag_dex = 1
               lst_dex = self.samplenum
           else:
                flag_dex = 0
                b_f = False
                times += 1  
                if times % 2 == 0:
                    self.put_row1(lst_dex, self.samplenum, self.samplenum -  lst_dex)
                else:
                    self.put_row2(lst_dex, self.samplenum, self.samplenum -  lst_dex)
                    #self.put_row3(lst_dex, self.samplenum, 0)

                 
           # wait({0:'f'}), 0表示通道序号(从0开始)，'f'表示查找下边沿
           # wait()可传多个条件，与条件:{0:'f',１:'r'},　或条件：[{0:'f'},{１:'r'}]
           # h:高电平，l:低电平，r:上边沿，f:下边沿，e:上沿或下沿, n:要么０，要么１
           # (a,b) = self.wait([{0:'f'}])
  
           step += 1
