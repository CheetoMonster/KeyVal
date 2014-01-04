
import distutils.core

mod = distutils.core.Extension('_KeyVal_C_API',
    sources=['python/KeyVal_wrap.c', 'KeyVal.c', 'KeyVal_load.c'],
    include_dirs=['.'],
    )

distutils.core.setup(
    name='KeyVal_C_API',
    ext_modules=[mod],
    py_modules=['KeyVal_C_API'],
    )

