from setuptools import setup

setup(
    name="lime-installer",
    version="0.1.0",
    description="LiMe OS Graphical Installer",
    author="LiMe OS Team",
    author_email="dev@lime-os.local",
    url="https://github.com/lime-os/lime-installer",
    license="GPL-2.0",
    py_modules=["installer", "installer_ui"],
    python_requires=">=3.8",
    install_requires=["PyQt5>=5.15.0"],
    entry_points={"console_scripts": ["lime-installer=installer:main"]},
)
