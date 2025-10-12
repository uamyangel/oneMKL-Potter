import sys

if __name__ == "__main__":
	fn = sys.argv[1]
	out_fn = fn.replace(".log", "")
	label = "FAIL"
	try:
		with open(fn) as f:
			num_net = 1
			num_routed_net = 0
			num_error = 1
			for line in f.readlines():
				if "# of routable nets" in line:
					num_net = int(line.split()[-2])
				if "# of fully routed nets" in line:
					num_routed_net = int(line.split()[-2])
				if "# of nets with routing errors" in line:
					num_error = int(line.split()[-2])
			if num_net == num_routed_net and num_error == 0:
				label = "PASS"
			print("num_net", num_net, "num_routed_net", num_routed_net, "num_error", num_error)
	except:
		pass
	with open(out_fn, 'w') as f:
		f.write(label)