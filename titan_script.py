import matplotlib.pyplot as plt

# Define x values (1-10)
x = [i for i in range(11)]

# Define y values (random data for simplicity)
y = [i**2 for i in range(11)]

# Create the figure and axis
fig, ax = plt.subplots()

# Plot the line graph
ax.plot(x, y)

# Set title and labels
ax.set_title('Simple Line Graph')
ax.set_xlabel('X Axis')
ax.set_ylabel('Y Axis')

# Save the graph as PNG
plt.savefig('graph.png')

# Close all open windows
plt.close()