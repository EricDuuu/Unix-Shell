if (cmdobj.in_file[0] != '\0') {
  int fd = open(cmdobj.in_file, O_RDONLY);
  dup2(fd, STDIN_FILENO);
  close(fd);
}
if (cmdobj.out_file[0] != '\0') {
  int fd = open(cmdobj.out_file, O_WRONLY | O_CREAT, 0644);
  dup2(fd, STDOUT_FILENO);
  close(fd);
}