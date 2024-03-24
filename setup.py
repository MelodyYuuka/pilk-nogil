from glob import glob

from setuptools import setup, Extension

SKP_SILK_SRC = 'src/SKP_SILK_SRC/'
sources = glob(SKP_SILK_SRC + '*.c') + glob('src/silk/*.c')
# noinspection SpellCheckingInspection
sources.append('src/pilkmodule.c')

# noinspection SpellCheckingInspection
pilkmodule = Extension(
    name='pilk_nogil._pilk',
    sources=sources,
    include_dirs=[SKP_SILK_SRC, 'src/interface', 'src/silk']
)

with open('README.md', encoding='utf8') as f:
    long_description = f.read()

# noinspection SpellCheckingInspection
setup(
    name='pilk-nogil',
    version='0.3.0',
    description='python silk voice library',
    long_description=long_description,
    long_description_content_type='text/markdown',
    author='foyou + MelodyYuuka',
    author_email='yimi.0822@qq.com + a876987@126.com',
    maintainer='MelodyYuuka',
    maintainer_email='a876987@126.com',
    url='https://github.com/MelodyYuuka/pilk',
    download_url='https://github.com/MelodyYuuka/releases',
    license_files=['LICENSE'],
    keywords=['silk', 'voice', 'python', 'extension', 'wechat', 'qq', 'tencent', 'xposed', 'c/c++'],
    python_requires='>=3.6',
    ext_modules=[pilkmodule],
    zip_safe=False,
    packages=['pilk_nogil'],
    package_data={
        'pilk_nogil': ['_pilk.pyi']
    },
    install_requires=['typing_extensions'],
    classifiers=[
        'Development Status :: 4 - Beta',
        'License :: OSI Approved :: GNU General Public License v3 (GPLv3)',
        'Intended Audience :: End Users/Desktop',
        'Natural Language :: Chinese (Simplified)',
        'Operating System :: Microsoft :: Windows',
        'Operating System :: POSIX',
        'Programming Language :: C',
        'Programming Language :: Python',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.6',
        'Programming Language :: Python :: 3.7',
        'Programming Language :: Python :: 3.8',
        'Programming Language :: Python :: 3.9',
        'Programming Language :: Python :: 3.10',
        'Programming Language :: Python :: 3.11',
        'Programming Language :: Python :: 3.12',
        'Topic :: Multimedia :: Sound/Audio'
    ],
)
