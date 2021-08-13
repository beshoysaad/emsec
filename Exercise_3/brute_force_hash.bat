@echo off

for /l %%x in (0, 1, 7) do (
	start cmd /k python hashtest.py 8 %%x
)