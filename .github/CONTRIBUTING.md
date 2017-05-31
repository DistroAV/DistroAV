##### Coding Guidelines
- Function and variable names: snake_case for C names, CamelCase for C++ names
- Tabs are 8 columns wide
- 80 columns max.

##### Commit Guidelines
- Commits follow the 50/72 standard:
	- 50 characters max for the title
	- One empty line after the title
	- Description wrapped to 72 columns max per line.
- Commit titles:
	- Use present tense
	- Prefix the title with a "scope" name
		- e.g: "CI: fix wrong behaviour when packaging for OS X"
		- Typical scopes: CI, General, Source, Output, Filter
		- Look at existing commits for more examples