"""Generate an original 20-section phase-splitting allpass pair (NumPy only).
Topology: H(z)=(p-z^-2)/(1-p*z^-2), with a one-sample delay on branch A.
Minimise quadrature error over 10 Hz .. Nyquist-10 Hz at 192 kHz.
Based on the allpass-pair topology described by Olli Niemitalo, not his coefficients.
"""
import numpy as np
n = 10
w = np.arctan(np.geomspace(np.tan(2*np.pi*10/192000), 1e5, 700))
t = np.tan(w)[:, None]
signs = np.tile([-1., 1.], n)
x = np.linspace(np.log(.8), np.log(.00003), n*2)
def evaluate(x):
    v=t/np.exp(x)
    phase=np.pi-2*np.arctan(v)
    error=phase@signs-w+np.pi/2
    jac=2*v/(1+v*v)*signs
    return error, jac
weights=np.ones(len(w))
for outer in range(7):
    damping=1e-4
    for i in range(180):
        err,jac=evaluate(x)
        cost=np.sum(weights*err*err)
        step=np.linalg.solve(jac.T@(weights[:,None]*jac)+damping*np.eye(2*n), -jac.T@(weights*err))
        trial=np.clip(x+step,-20,0)
        new,_=evaluate(trial)
        if np.sum(weights*new*new)<cost:
            x=trial; damping=max(1e-12,damping*.5)
        else: damping=min(1e5,damping*5)
    err,_=evaluate(x)
    weights=np.maximum(.05, (np.abs(err)/max(np.abs(err)))**2)
coeff=(1-np.exp(x))/(1+np.exp(x))
print('A:', ', '.join(f'{v:.17g}' for v in coeff[1::2]))
print('B:', ', '.join(f'{v:.17g}' for v in coeff[0::2]))
print('Worst image rejection, dB:',20*np.log10(np.max(np.abs(np.tan(evaluate(x)[0]/2)))))
