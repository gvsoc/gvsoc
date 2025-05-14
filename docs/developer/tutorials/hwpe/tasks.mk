create_target_task1:
	$(MAKE) SRC_FPATH=$(SUBTASKS_MK_DIR)create_target/task_setup/pulp-open.py DEST_FPATH=$(SUBTASKS_MK_DIR)../../../../pulp/pulp-open.py copy_file
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)create_target/task_setup/pulp_open/ DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/chips/pulp_open/ copy_folder_contents

integrate_hwpe_task1:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)integrate_hwpe/task_files/ DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/chips/pulp_open_hwpe copy_folder
integrate_hwpe_task2:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)integrate_hwpe/task1/ DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/chips/pulp_open_hwpe copy_folder
integrate_hwpe_task3:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)integrate_hwpe/task2/ DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/chips/pulp_open_hwpe copy_folder
integrate_hwpe_task4:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)integrate_hwpe/task3/ DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/chips/pulp_open_hwpe copy_folder
integrate_hwpe_task5:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)integrate_hwpe/task4/ DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/chips/pulp_open_hwpe copy_folder
integrate_hwpe_task6:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)integrate_hwpe/task5/ DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/chips/pulp_open_hwpe copy_folder
integrate_hwpe_task7:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)integrate_hwpe/task6/ DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/chips/pulp_open_hwpe copy_folder
integrate_hwpe_task8:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)integrate_hwpe/task7/ DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/chips/pulp_open_hwpe copy_folder
integrate_hwpe_task9:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)integrate_hwpe/task8/ DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/chips/pulp_open_hwpe copy_folder
integrate_hwpe_task10:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)integrate_hwpe/task9/ DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/chips/pulp_open_hwpe copy_folder
integrate_hwpe_task11:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)integrate_hwpe/task10/ DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/chips/pulp_open_hwpe copy_folder

model_hwpe_task1:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)model_hwpe/task_files/task1/simple_hwpe DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/simple_hwpe replace_folder
# model_hwpe_task2:
# 	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)model_hwpe/task_files/task2/simple_hwpe DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/simple_hwpe replace_folder
# model_hwpe_task3:
# 	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)model_hwpe/task_files/task3/simple_hwpe DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/simple_hwpe replace_folder
model_hwpe_task2:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)model_hwpe/task_files/task4/simple_hwpe DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/simple_hwpe replace_folder
# model_hwpe_task3:
# 	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)model_hwpe/task_files/task5/simple_hwpe DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/simple_hwpe replace_folder
model_hwpe_task3:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)model_hwpe/task_files/task6/simple_hwpe DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/simple_hwpe replace_folder
model_hwpe_task4:
	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)model_hwpe/task_files/task7/simple_hwpe DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/simple_hwpe replace_folder
# model_hwpe_task8:
# 	$(MAKE) SRC_DIR=$(SUBTASKS_MK_DIR)model_hwpe/task_files/task8/simple_hwpe DEST_DIR=$(SUBTASKS_MK_DIR)../../../../pulp/pulp/simple_hwpe replace_folder
