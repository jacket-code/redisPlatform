#!/usr/bin/python
# -------_*_ coding: utf8 _*_
# author: jacketzhong
# date: 2015

import os
import sys
import logging
from time import *

logging.basicConfig(level = logging.DEBUG,
                    format = '%(asctime)s %(filename)s[line:%(lineno)d] %(levelname)s %(message)s',
                    filename = 'gen.log')

if __name__ == '__main__':
    reload(sys)
    sys.setdefaultencoding('utf8')
    os.chdir(sys.path[0])
    logging.info('LoaderScrip START AT %s', strftime("%Y-%m-%d %H:%M:%S", localtime(int(time()))))
    types = sys.argv[1]
    soname = sys.argv[2]
    logging.info('argv1 %s, argv2 %s ', types, soname)

    # TODO: COPY file

