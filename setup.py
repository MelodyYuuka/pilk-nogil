from glob import glob
from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext

SKP_SILK_SRC = "src/SKP_SILK_SRC/"
sources = glob(SKP_SILK_SRC + "*.c") + glob("src/silk/*.c")
sources.append("src/pilkmodule.c")


class CustomBuildExt(build_ext):
    def build_extensions(self):
        compiler_type = self.compiler.compiler_type

        if compiler_type == "msvc":
            extra_compile_args = ["/O2", "/GL"]
            extra_link_args = ["/LTCG"]
        else:
            extra_compile_args = ["-O3", "-flto"]
            extra_link_args = ["-flto"]

        for ext in self.extensions:
            ext.extra_compile_args.extend(extra_compile_args)
            ext.extra_link_args.extend(extra_link_args)

        super().build_extensions()


pilkmodule = Extension(
    name="pilk_nogil._pilk",
    sources=sources,
    include_dirs=[SKP_SILK_SRC, "src/interface", "src/silk"],
)

setup(ext_modules=[pilkmodule], cmdclass={"build_ext": CustomBuildExt})
