NAME :=	VULKAN_TUTORIAL

all : $(NAME)

$(NAME):
	@cmake -Bbuild . 
	@cmake --build build --config Debug
	@mv ./build/Debug/VULKAN_TUTORIAL.exe ./VULKAN_TUTORIAL.exe
	@echo [SUCCESS] $@ compiled successfully!

clean :
	@rm -rf ./build/
	@echo [CLEAN] Object files have been removed!

fclean : clean
	@rm -rf VULKAN_TUTORIAL.exe
	@echo [FCLEAN] Executable files have been fully removed!

re : fclean all

.PHONY : all clean fclean re