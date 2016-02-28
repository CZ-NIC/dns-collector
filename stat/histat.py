#!/usr/bin/python

import numpy as np
import pandas
import scipy
import glob
import matplotlib.pyplot as plt

import scipy.stats 
#from sklearn.neighbors import KernelDensity
from sklearn.decomposition import PCA, NMF

def read_csv_file(fname):
    d = pandas.read_csv(fname, sep='|', header=0)
    d['delay'] = d['response_time_us'] - d['request_time_us']
    return d

#def dists(d):
#    dists = {}
#    bins = {}
#    dists['delay'], bins['delay'] = np.histogram(d['delay'].valid(), density=False, bins=[10*1.5**i for i in range(30)])
#    return dists['delay']

xs = np.linspace(0, 1000, num=1000)
xsl = np.linspace(0, 12, num=1000)

ds = [read_csv_file(fn)['delay'].valid() for fn in glob.glob('out/akuma*.csv')]
dsl = [np.log(i) for i in ds]
ks = [scipy.stats.gaussian_kde(i) for i in ds]
ksl = [scipy.stats.gaussian_kde(i) for i in dsl]
ys = [i(xs) for i in ks]
ysl = [i(xsl) for i in ksl]

nc = 2
fp=PCA(n_components=nc).fit(ys)
fpl=PCA(n_components=nc).fit(ysl)
fn=NMF(n_components=nc).fit(ys)
fnl=NMF(n_components=nc).fit(ysl)

for i in ysl:
    plt.plot(xsl, i, '.', color='gray')
plt.plot(xsl, np.average(ysl, axis=0), 'r--')
for i in fnl.components_:
    plt.plot(xsl, i, '-')
plt.show()

