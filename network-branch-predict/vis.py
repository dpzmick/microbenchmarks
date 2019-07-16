import numpy as np
from matplotlib import pyplot as plt

branching = np.fromfile("res_a", dtype=np.uint64)
#print(branching)

branch_free = np.fromfile("res_b", dtype=np.uint64)
#print(branch_free)

# ans = np.fromfile("answer", dtype=np.uint64)
# reps = branch_free.shape[0]/ans.shape[0]
# print(reps)

# ans = np.repeat(ans, int(reps))

# plt.scatter(, len(branch_free)), branch_free)
# plt.scatter(np.arange(0, len(branching)), branching)
# plt.show()

# plt.hist(branching, density=True, range=[0, 500])
# plt.hist(branch_free, density=True, range=[0, 500])
# plt.show()

print('branching mean', np.mean(branching))
print('branching min', np.min(branching))
print('branching max', np.max(branching))
print('branching var', np.var(branching))
print('')
print('branch-free mean', np.mean(branch_free))
print('branch-free min', np.min(branch_free))
print('branch-free max', np.max(branch_free))
print('branch-free var', np.var(branch_free))
print('')
print('branch-free %0.3f times slower than branching' %
      (np.mean(branch_free)/(np.mean(branching))))
print('braching variance %0.3f times larger than branch-free' %
      (np.var(branching)/np.var(branch_free)))
