# Ensure that the path where subtasks.mk is located is captured
SUBTASKS_MK_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

# Function to copy contents of one folder to another
define copy_folder_contents
	@echo "Copying contents from $(1) to $(2)..."
	@cp -r $(1)/* $(2)/
endef

# Function to copy a file from one location to another
define copy_file
	@echo "Copying file from $(1) to $(2)..."
	@cp $(1) $(2)
endef


# Function to replace the destination folder with the source folder
define replace_folder
	@echo "Replacing contents of $(2) with $(1)..."
	@rm -rf $(2)
	@cp -r $(1) $(2)
endef

# Function to check if a directory exists and create it if it does not
define check_and_create_dir
	@if [ ! -d "$(1)" ]; then \
	    echo "Directory $(1) does not exist. Creating..."; \
	    mkdir -p $(1); \
	else \
	    echo "Directory $(1) already exists."; \
	fi
endef

# The copy_folder target
copy_folder:
	$(call copy_folder_contents,$(SRC_DIR),$(DEST_DIR))

# The copy_file target
copy_file:
	$(call copy_file,$(SRC_FPATH),$(DEST_FPATH))

# The replace_folder target
replace_folder:
	$(call replace_folder,$(SRC_DIR),$(DEST_DIR))

# Print SUBTASKS_MK_DIR
print_subtasks_mk_dir:
	@echo "SUBTASKS_MK_DIR: $(SUBTASKS_MK_DIR)"


# The replace_folder target
copy_folder_contents:
	$(call copy_folder_contents,$(SRC_DIR),$(DEST_DIR))