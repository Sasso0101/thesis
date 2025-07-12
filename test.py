from statistics import geometric_mean


def parse_stdout(stdout: str) -> float:
    lines = stdout.split('\n')
    times = []
    for i in range(2, len(lines), 2):
        times.append(float(lines[i].split(',')[-1]))
    print(times)
    return geometric_mean(times)


sout = '''
run_id=0,diameter=20131,threads=1,chunk_size=4096,max_chunks=4,1.6930
run_id=1,diameter=17633,threads=1,chunk_size=4096,max_chunks=3,1.7029
run_id=2,diameter=22275,threads=1,chunk_size=4096,max_chunks=3,1.6902
run_id=3,diameter=17103,threads=1,chunk_size=4096,max_chunks=3,1.7147
run_id=4,diameter=18355,threads=1,chunk_size=4096,max_chunks=3,1.7343
run_id=5,diameter=17429,threads=1,chunk_size=4096,max_chunks=3,1.7154
run_id=6,diameter=24565,threads=1,chunk_size=4096,max_chunks=3,1.6545
run_id=7,diameter=17914,threads=1,chunk_size=4096,max_chunks=3,1.7162
run_id=8,diameter=17193,threads=1,chunk_size=4096,max_chunks=3,1.6942
run_id=9,diameter=15851,threads=1,chunk_size=4096,max_chunks=3,1.6996
'''

print(parse_stdout(sout))