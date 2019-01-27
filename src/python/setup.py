from distutils.core import setup, Extension

opt = ['-Wall -Wuninitialized -Wstrict-prototypes -Wno-pointer-sign']

incdirs = ['../../include']

module1 = Extension('helios',
                    sources=['helios.c'],
                    include_dirs = incdirs,
                    libraries=["syscall"],
                    extra_compile_args = opt)

setup(  name = 'Helios',
        version = '0.1',
        author      = 'Guillaume ROGUEZ', 
        description = 'Python module wrapper for the Helios library',
        ext_modules = [module1])

