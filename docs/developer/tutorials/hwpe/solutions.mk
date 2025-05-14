
# include /helpers.mk

create_target_sol_task1:
	$(MAKE) SRC_FPATH=$(SUBTASKS_MK_DIR)../../../../pulp/pulp-open.py DEST_FPATH=$(SUBTASKS_MK_DIR)../../../../pulp/pulp-open-hwpe.py copy_file

create_target_sol_task2: create_target_sol_task1
	$(call check_and_create_dir,$(SUBTASKS_MK_DIR)../../../../pulp/pulp/chips/pulp_open_hwpe)
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/chips/pulp_open DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/chips/pulp_open_hwpe copy_folder

create_target_sol_task3: create_target_sol_task2
	$(MAKE) SRC_FPATH=$(SUBTASKS_MK_DIR)create_target/solutions/task3/pulp-open-hwpe.py DEST_FPATH=$(SUBTASKS_MK_DIR)../../../../pulp/ copy_file

create_target_sol_task4: create_target_sol_task3
	$(call check_and_create_dir,$(SUBTASKS_MK_DIR)../../../../pulp/pulp/chips/pulp_open_hwpe)
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)create_target/solutions/task4 DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/chips/pulp_open_hwpe copy_folder



integrate_hwpe_sol1:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)integrate_hwpe/task1/ DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/chips/pulp_open_hwpe copy_folder
integrate_hwpe_sol2:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)integrate_hwpe/task2/ DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/chips/pulp_open_hwpe copy_folder
integrate_hwpe_sol3:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)integrate_hwpe/task3/ DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/chips/pulp_open_hwpe copy_folder
integrate_hwpe_sol4:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)integrate_hwpe/task4/ DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/chips/pulp_open_hwpe copy_folder
integrate_hwpe_sol5:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)integrate_hwpe/task5/ DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/chips/pulp_open_hwpe copy_folder
integrate_hwpe_sol6:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)integrate_hwpe/task6/ DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/chips/pulp_open_hwpe copy_folder
integrate_hwpe_sol7:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)integrate_hwpe/task7/ DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/chips/pulp_open_hwpe copy_folder
integrate_hwpe_sol8:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)integrate_hwpe/task8/ DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/chips/pulp_open_hwpe copy_folder
integrate_hwpe_sol9:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)integrate_hwpe/task9/ DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/chips/pulp_open_hwpe copy_folder
integrate_hwpe_sol10:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)integrate_hwpe/solutions/pulp_open_hwpe DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/chips/pulp_open_hwpe copy_folder
	$(MAKE) SRC_FPATH=$(SUBTASKS_MK_DIR)integrate_hwpe/solutions/CMakeLists.txt DEST_FPATH=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/ copy_file 

model_hwpe_sol1:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)model_hwpe/solutions/task1/simple_hwpe DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/simple_hwpe replace_folder
model_hwpe_sol2:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)model_hwpe/solutions/task2/simple_hwpe DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/simple_hwpe replace_folder
model_hwpe_task3_pre:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)model_hwpe/solutions/task3/simple_hwpe DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/simple_hwpe replace_folder
model_hwpe_sol4:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)model_hwpe/solutions/task4/simple_hwpe DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/simple_hwpe replace_folder
model_hwpe_sol5:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)model_hwpe/solutions/task5/simple_hwpe DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/simple_hwpe replace_folder
model_hwpe_sol6:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)model_hwpe/solutions/task6/simple_hwpe DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/simple_hwpe replace_folder
model_hwpe_sol7:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)model_hwpe/solutions/task7/simple_hwpe DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/simple_hwpe replace_folder
model_hwpe_sol8:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)model_hwpe/solutions/task8/simple_hwpe DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/simple_hwpe replace_folder