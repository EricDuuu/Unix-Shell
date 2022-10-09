{ pkgs }: {
	deps = [
		pkgs.wget
  pkgs.clang_12
		pkgs.ccls
		pkgs.gdb
		pkgs.gnumake
	];
}