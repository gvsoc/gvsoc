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

integrate_hwpe_task1:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)integrate_hwpe/task_files/ DEST_DIR=$(SUBTASKS_MK_DIR)../pulp/pulp/chips/pulp_open_hwpe copy_folder
integrate_hwpe_task2:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)integrate_hwpe/task1/ DEST_DIR=$(SUBTASKS_MK_DIR)../pulp/pulp/chips/pulp_open_hwpe copy_folder
integrate_hwpe_task3:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)integrate_hwpe/task2/ DEST_DIR=$(SUBTASKS_MK_DIR)../pulp/pulp/chips/pulp_open_hwpe copy_folder
integrate_hwpe_task4:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)integrate_hwpe/task3/ DEST_DIR=$(SUBTASKS_MK_DIR)../pulp/pulp/chips/pulp_open_hwpe copy_folder
integrate_hwpe_task5:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)integrate_hwpe/task4/ DEST_DIR=$(SUBTASKS_MK_DIR)../pulp/pulp/chips/pulp_open_hwpe copy_folder
integrate_hwpe_task6:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)integrate_hwpe/task5/ DEST_DIR=$(SUBTASKS_MK_DIR)../pulp/pulp/chips/pulp_open_hwpe copy_folder
integrate_hwpe_task7:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)integrate_hwpe/task6/ DEST_DIR=$(SUBTASKS_MK_DIR)../pulp/pulp/chips/pulp_open_hwpe copy_folder
integrate_hwpe_task8:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)integrate_hwpe/task7/ DEST_DIR=$(SUBTASKS_MK_DIR)../pulp/pulp/chips/pulp_open_hwpe copy_folder
integrate_hwpe_task9:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)integrate_hwpe/task8/ DEST_DIR=$(SUBTASKS_MK_DIR)../pulp/pulp/chips/pulp_open_hwpe copy_folder
integrate_hwpe_task10:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)integrate_hwpe/task9/ DEST_DIR=$(SUBTASKS_MK_DIR)../pulp/pulp/chips/pulp_open_hwpe copy_folder
integrate_hwpe_task11:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)integrate_hwpe/task10/ DEST_DIR=$(SUBTASKS_MK_DIR)../pulp/pulp/chips/pulp_open_hwpe copy_folder

model_hwpe_task1:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)model_hwpe/task_files/task1/simple_hwpe DEST_DIR=$(SUBTASKS_MK_DIR)../pulp/pulp/simple_hwpe replace_folder
model_hwpe_task2:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)model_hwpe/task_files/task2/simple_hwpe DEST_DIR=$(SUBTASKS_MK_DIR)../pulp/pulp/simple_hwpe replace_folder
model_hwpe_task3:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)model_hwpe/task_files/task3/simple_hwpe DEST_DIR=$(SUBTASKS_MK_DIR)../pulp/pulp/simple_hwpe replace_folder
model_hwpe_task4:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)model_hwpe/task_files/task4/simple_hwpe DEST_DIR=$(SUBTASKS_MK_DIR)../pulp/pulp/simple_hwpe replace_folder
model_hwpe_task5:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)model_hwpe/task_files/task5/simple_hwpe DEST_DIR=$(SUBTASKS_MK_DIR)../pulp/pulp/simple_hwpe replace_folder
model_hwpe_task6:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)model_hwpe/task_files/task6/simple_hwpe DEST_DIR=$(SUBTASKS_MK_DIR)../pulp/pulp/simple_hwpe replace_folder
model_hwpe_task7:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)model_hwpe/task_files/task7/simple_hwpe DEST_DIR=$(SUBTASKS_MK_DIR)../pulp/pulp/simple_hwpe replace_folder
model_hwpe_task8:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)model_hwpe/task_files/task8/simple_hwpe DEST_DIR=$(SUBTASKS_MK_DIR)../pulp/pulp/simple_hwpe replace_folder
