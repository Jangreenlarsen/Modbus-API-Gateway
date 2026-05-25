from setuptools import setup

setup(
    name="mbgw",
    version="1.0.0",
    py_modules=["mbgw"],
    install_requires=["click>=8.0", "requests>=2.28"],
    entry_points={"console_scripts": ["mbgw=mbgw:cli"]},
)
