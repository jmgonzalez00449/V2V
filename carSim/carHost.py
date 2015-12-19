#!/usr/bin/env python3.3

"""
Simulates a car using symba 
to optimize car cluster movement
"""

import asyncore
import re
import subprocess
from threading import Thread, Lock, Event
from time import sleep
from carDefs import *



class CarHandler(asyncore.dispatcher_with_send):

	def __init__(self, *args, master=None, carIdx=0, carListLock=Lock(), carList={}, **kwargs):
		asyncore.dispatcher_with_send.__init__(self, *args, **kwargs)

		self.carListLock = carListLock
		self.carList = carList	
		self.carIdx = carIdx
		self.master = master

		print("__init__: " + str(self.socket) )

	def handle_read(self):

		data = self.recv(8192)

		if data:

			print("socket: " + str(self.socket) + " data=" + str(data))

			# Parse message
			data = data.decode('utf-8')
			
			if data[0] == 'G':
				if self.master:
					maxVelocity = self.master.calculateVelocity(self.carIdx)	
					self.send((VELOCITY_TAG+"="+maxVelocity).encode('utf-8'))
					return


			locationSearch = re.search(LOCATION_TAG+"=([^,]*),",data)
			if locationSearch:
				location = locationSearch.group(1)
			else:
				location = "NO DATA"

			velocitySearch = re.search(VELOCITY_TAG+"=([^,]*),",data)
			if velocitySearch:
				velocity = velocitySearch.group(1)
			else:
				velocity = "NO DATA"

			destinationSearch = re.search(DESTINATION_TAG+"=([^,]*),",data)
			if destinationSearch:
				destination = destinationSearch.group(1)
			else:
				destination = "NO DATA"			
		
			with self.carListLock:

				updatedEntry = {	
							"location":location,
							"velocity":velocity,
							"destination":destination,
						}
						
				self.carList[self.carIdx].update(updatedEntry)

	def handle_close(self):

		with self.carListLock:

			if self.carIdx in self.carList:
				del self.carList[self.carIdx]

class ClusterMaster( asyncore.dispatcher ):

	def __init__(self, host, port):
		asyncore.dispatcher.__init__(self)
		self.create_socket()
		self.set_reuse_addr()
		self.bind((host,port))
		self.listen(5)

		self.carListLock = Lock()
		self.carList = {}

		self.nextCarIdx = 0

	def handle_accepted(self, sock, addr):
		print('Incoming connection from {0}'.format(addr))
		with self.carListLock:
			self.carList[self.nextCarIdx] = {}

		handler = CarHandler(sock, master=self, carIdx=self.nextCarIdx, carListLock=self.carListLock, carList=self.carList)

		self.nextCarIdx += 1

	def calculateVelocity(self, carIdxToOptimize):

		variables = []
		entries = []

		with self.carListLock:

			for carIdx,carDict in self.carList.items():

				newEntries, newVariables = makeMovementEntries( carIdx, 
									carDict["velocity"],
									carDict["location"],
									carDict["destination"])

				entries += newEntries
				variables += list(newVariables)

		
		symbaFileString = ""
	
		for variable in variables:
			symbaFileString += "(declare-const " + variable + " Real)\n"

		symbaFileString += "(assert (=>\n\t(and\n"
	
		for entry in entries:

			for line in entry.split('\n'):
				symbaFileString += "\t\t" + line + "\n"

		symbaFileString += "\t)\n"

		# The things to max/minimize
		symbaFileString += "\t(and\n"

		# Maximize velocity
		symbaFileString += "\t\t(= v" + str(carIdxToOptimize) + " v" + str(carIdxToOptimize) + ")\n"

		symbaFileString += "\t)\n"
	
		# End the (=>	
		symbaFileString += "\t)\n"

		# End the (assert
		symbaFileString += ")\n"

		print(symbaFileString)

		with open("smtFile.smt","w+") as file:
			file.write(symbaFileString)

		# Run Symba
		symbaOutput = subprocess.check_output(["/home/xion/Documents/cs527/symba/build/ufo-prefix/bin/symba", "-b=smtFile.smt"], universal_newlines=True, stderr=subprocess.STDOUT)

		print("symbaOutput: \n" + symbaOutput)

		# Find the max velocity for this car
		velocityResultSearch = re.search(r"RESULT:[ ]*v"+str(carIdx) + "[ :]*\[([^,]*),([^]]*)", symbaOutput)
		if velocityResultSearch:
			velocityMax = velocityResultSearch.group(2)
		else:
			velocityMax = "NO DATA"

		return velocityMax


def makeMovementEntries( carIdx, velocity, location, destination ):

	motionEntry = "(= d" + str(carIdx) + " (+ (* v" + str(carIdx) + " t" + str(carIdx) + ") " \
			+ " " + location + "))" 

	timeEntry = "(>= t" + str(carIdx) + " 0)"

	velocityEntry = "(>= v" + str(carIdx) + " 0)\n" \
			+ "(<= v" + str(carIdx) + " " + velocity + ")"

	destinationEntry = "(= d" + str(carIdx) + " " + destination + ")"

	entries = [motionEntry, timeEntry, velocityEntry, destinationEntry]

	return entries, ("d"+str(carIdx), "t"+str(carIdx), "v"+str(carIdx))

class CarListThread( Thread ):

	def __init__(self, carListLock=Lock(), carList={}, exitEvent=Event()):

		Thread.__init__(self)
		self.carListLock = carListLock
		self.carList = carList

		self.exitEvent = exitEvent

	def run(self):

		while not self.exitEvent.isSet():


			sleep(5)

			with self.carListLock:

				print("carList =  " + str(self.carList))	

if __name__ == "__main__":

	exitEvent = Event()
	clusterMaster = ClusterMaster('', 8080)
	carListThread = CarListThread(clusterMaster.carListLock, clusterMaster.carList, exitEvent)
	carListThread.start()
	asyncore.loop()	
