import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import sys
import os
import importlib.util

bLoadFactor= False if len(sys.argv) < 4 else True

def Show(data, window_title):

	#fig = plt.figure(figsize=(9.7, 6.3))
	fig = plt.figure(figsize=(12, 7))
	fig.canvas.manager.set_window_title(window_title + '.py')

	if not bLoadFactor:
		#src_xticks = (1,2,3,4,5,6,7,8,9,10)
		#xticks = [it * 100000 for it in src_xticks]
		#xlabels = [str(it) for it in src_xticks]

		src_xticks = (1,8,16,32,48,64,70,80,90,100,110,120,128)
		xticks = [it * 1000 * 1000 for it in src_xticks]
		xlabels = [str(it) for it in src_xticks]
	else:
		#mv = 1024 * 1024
		#src_xticks = (0.1,0.15,0.2,0.25,0.3,0.35,0.4,0.45,0.5,0.55,0.6,0.65,0.7,0.75,0.8,0.85,0.9,1000000/(1024*1024))
		#xticks = [it * 100000 if it >= 1 else it*mv for it in src_xticks]
		#xlabels = [str(it if it >= 1 else round(it,4)) for it in src_xticks]

		mv = 128 * 1024 * 1024
		src_xticks = (1,4,8,0.1,0.15,0.2,0.25,0.3,0.35,0.4,0.45,0.5,0.55,0.6,0.65,0.7,0.75,0.8,0.85,0.9,1000000/(1024*1024))
		xticks = [it * 1024 * 1024 if it >= 1 else it*mv for it in src_xticks]
		xlabels = [str(it if it >= 1 else round(it,4)) for it in src_xticks]


	manager = plt.get_current_fig_manager()
	#manager.window.wm_geometry("+0+0")
 
	plt.subplots_adjust(left=0.07,right=0.97,top=0.97,bottom=0.05,hspace=0.11, wspace=0.11)

	ax = plt.subplot(2, 2, 1)
	ax.set_xticks(xticks, labels=xlabels, fontsize=6)
	plt.yticks(fontsize=7)

	leg, lines = create('time', data, ax)
	#leg.set_draggable(True)

	map_legend_to_ax = {}  # Will map legend lines to original lines.

	i = 0
	for legend_line, ax_line in zip(leg.get_patches(), lines):
		legend_line.set_picker(5)
		map_legend_to_ax[legend_line] = [ax_line, ax, i]
		i = i + 1
		
	def on_pick(event):
		legend_line = event.artist
		if legend_line not in map_legend_to_ax:
			return

		ax_line, ax0, i = map_legend_to_ax[legend_line]

		for rec in map_legend_to_ax:
			ax_line, ax0, _i = map_legend_to_ax[rec]
			if i==_i:
				visible = not ax_line.get_visible()
				ax_line.set_visible(visible)
		
				legend_line.set_alpha(1.0 if visible else 0.2)
				ax0.figure.canvas.draw()

	ax.figure.canvas.mpl_connect('pick_event', on_pick)
 
	plt.title(data[0][0][1], fontsize=7)
	plt.ylabel("time (sec)", fontsize=8)
	plt.grid(True)

	ax = plt.subplot(2, 2, 2)
	ax.set_xticks(xticks, labels=xlabels, fontsize=6)
	plt.yticks(fontsize=6)
	#plt.title("memory (mb)", fontsize=7)
	plt.title(data[0][0][2] + ", mem(mb)", fontsize=7)

	leg, lines = create('mem', data, ax)

	i = 0
	for legend_line, ax_line in zip(leg.get_patches(), lines):
		legend_line.set_picker(5)
		map_legend_to_ax[legend_line] = [ax_line, ax, i]
		i = i + 1

	#ax.figure.canvas.mpl_connect('pick_event', on_pick)
	plt.grid(True)
	leg.set_visible(False)

	ax = plt.subplot(2, 1, 2)
	ax.set_xticks(xticks, labels=xlabels, fontsize=6)
	plt.yticks(fontsize=7)
	plt.ylabel("operation time (ns)", fontsize=7)

	leg, lines = create('op', data, ax)
	
	i = 0
	for legend_line, ax_line in zip(leg.get_patches(), lines):
		legend_line.set_picker(5)
		map_legend_to_ax[legend_line] = [ax_line, ax, i]
		i = i + 1

	#ax.figure.canvas.mpl_connect('pick_event', on_pick)
	plt.grid(True)
	leg.set_visible(False)

	plt.savefig('.\\' + window_title + '.png', dpi=600, bbox_inches='tight')
	plt.show()

def	create(name, data, ax):

	colors = ['blue', 'green', 'red', 'orange', 'black', 'purple', 'brown', 'olive', 'magenta']
	#colors = plt.get_cmap('tab10').colors[:10]
	#colors = plt.get_cmap('Dark2').colors[:10]
	#colors = plt.get_cmap('Set1').colors[:10]
	
	cnt = [entry["cnt"] for entry in data[0][1]]

	handles = []; lines = []

	for i, dentry in enumerate(data):
		handles.append(mpatches.Patch(color=colors[i], label=data[i][0][0]))
		line,  = ax.plot(cnt, [entry[name] for entry in dentry[1]], marker='', linestyle=':', color=colors[i])
		lines.append(line)

	return ax.legend(handles=handles, loc='upper left', fontsize=7, fancybox=True, shadow=True), lines

def load_data(name):
	data=[]; module = importlib.import_module(name)
	for i in range(1, 10):
		if hasattr(module, 'data' + str(i)):
			data.append(getattr(module, 'data' + str(i)))

	return data

if __name__ == '__main__':
	sys.path.append(os.path.abspath(sys.argv[1]))
	for test in ['random']:
		for i in range(1, 5):
			name = 'test' + str(i) + '_' + sys.argv[2] + '_' + test
			file = '.\\' + sys.argv[1] + '\\' + name + '.py'
			if os.path.exists(file):
				Show(load_data(name), sys.argv[1] + '\\' + name)
