from setuptools import setup, find_packages

setup(
    name="lime-installer",
    version="0.1.0",
    description="LiMe OS Graphical Installer",
    author="LiMe OS Team",
    author_email="dev@lime-os.local",
    url="https://github.com/lime-os/lime-installer",
    license="GPL-2.0",
    packages=find_packages(where="src"),
    package_dir={"": "src"},
    python_requires=">=3.8",
    install_requires=[
        "PyQt5>=5.15.0",
    ],
    entry_points={
        "console_scripts": [
            "lime-installer=installer:main",
        ],
    },
    classifiers=[
        "Development Status :: 3 - Alpha",
        "Environment :: X11 Applications :: Qt",
        "License :: OSI Approved :: GNU General Public License v2 (GPLv2)",
        "Operating System :: POSIX :: Linux",
        "Programming Language :: Python :: 3",
    ],
)
